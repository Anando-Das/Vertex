import os
import re
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from flask import Flask, jsonify, render_template, request

GUI_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = GUI_DIR.parent

app = Flask(__name__, template_folder=str(GUI_DIR / "templates"))

SECTION_RE = re.compile(
    r"^=== (Abstract Syntax Tree|Semantic Analysis|Symbol Table|Three Address Code \(TAC\))(?:\s*\([^)]*\))? ===\s*$",
    re.MULTILINE,
)

ERROR_LINE_RE = re.compile(
    r"^(Lexical Error|Syntax Error|Semantic Error)\s+at\s+line\s+(\d+):\s*(.+)$"
)

GPP_DIAG_RE = re.compile(
    r"^(?:.+?[/\\])?main\.cpp:(\d+)(?::(\d+))?:\s*(fatal error|error|warning):\s*(.+)$"
)

JAVAC_DIAG_RE = re.compile(
    r"^(?:.+?[/\\])?Main\.java:(\d+):\s*(error|warning):\s*(.+)$"
)

RUN_TIMEOUT_SECONDS = 5
COMPILE_TIMEOUT_SECONDS = 15


def find_compiler_binary() -> Path | None:
    """Locate the compiled Vertex front-end binary in the project root."""
    candidates = (
        [PROJECT_ROOT / "compiler.exe", PROJECT_ROOT / "compiler"]
        if os.name == "nt"
        else [PROJECT_ROOT / "compiler", PROJECT_ROOT / "compiler.exe"]
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def find_tool(tool_name: str, windows_candidates: list[str] | None = None) -> Path | None:
    """Find an external compiler/runtime executable on PATH or common Windows locations."""
    found = shutil.which(tool_name)
    if found:
        return Path(found)

    if os.name == "nt" and windows_candidates:
        for raw_path in windows_candidates:
            p = Path(raw_path)
            if p.is_file():
                return p
    return None


def find_gpp() -> Path | None:
    return find_tool(
        "g++",
        [
            r"C:\msys64\ucrt64\bin\g++.exe",
            r"C:\msys64\mingw64\bin\g++.exe",
            r"C:\msys64\clang64\bin\g++.exe",
            r"C:\MinGW\bin\g++.exe",
            r"C:\mingw64\bin\g++.exe",
        ],
    )


def find_jdk_tool(name: str) -> Path | None:
    candidates: list[str] = []
    if os.name == "nt":
        for root_dir in (r"C:\Program Files\Java", r"C:\Program Files\Eclipse Adoptium"):
            root_path = Path(root_dir)
            if root_path.is_dir():
                for jdk_dir in sorted(root_path.iterdir(), reverse=True):
                    exe = jdk_dir / "bin" / f"{name}.exe"
                    if exe.is_file():
                        candidates.append(str(exe))
    return find_tool(name, candidates)


def classify_sample(rel_path: str) -> tuple[int, str]:
    """Return (sort_order, category_label) for grouping Vertex samples in the UI dropdown."""
    if rel_path.startswith("examples/"):
        return (0, "Examples")
    if rel_path.startswith("tests/semantic_errors/"):
        return (3, "Semantic Errors")
    if "error" in rel_path.lower():
        return (2, "Lexical & Syntax Errors")
    return (1, "Valid Tests")


def parse_compiler_stdout(stdout_text: str) -> dict[str, str]:
    """Split Vertex compiler stdout into structured AST, Semantic Analysis, Symbol Table, and TAC sections."""
    sections = {
        "ast": "",
        "semantic": "",
        "symbol_table": "",
        "tac": "",
    }
    if not stdout_text.strip():
        return sections

    key_map = {
        "Abstract Syntax Tree": "ast",
        "Semantic Analysis": "semantic",
        "Symbol Table": "symbol_table",
        "Three Address Code (TAC)": "tac",
    }

    matches = list(SECTION_RE.finditer(stdout_text))
    for idx, match in enumerate(matches):
        header_name = match.group(1)
        start = match.end()
        end = matches[idx + 1].start() if idx + 1 < len(matches) else len(stdout_text)
        body = stdout_text[start:end].strip("\r\n")
        key = key_map.get(header_name)
        if key:
            sections[key] = body

    return sections


def parse_compiler_errors(stderr_text: str) -> tuple[list[dict], str]:
    """Extract structured error entries and the final summary message from Vertex stderr."""
    errors: list[dict] = []
    summary_lines: list[str] = []

    for raw_line in stderr_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        match = ERROR_LINE_RE.match(line)
        if match:
            errors.append(
                {
                    "kind": match.group(1),
                    "line": int(match.group(2)),
                    "message": match.group(3),
                    "raw": line,
                }
            )
        else:
            summary_lines.append(line)

    return errors, "\n".join(summary_lines)


def parse_gpp_errors(stderr_text: str) -> list[dict]:
    """Parse g++ compiler diagnostics into structured error objects."""
    errors: list[dict] = []
    for raw_line in stderr_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        m = GPP_DIAG_RE.match(line)
        if m:
            severity = m.group(3).lower()
            if "error" in severity:
                errors.append(
                    {
                        "kind": "C++ Compile Error",
                        "line": int(m.group(1)),
                        "message": m.group(4),
                        "raw": line,
                    }
                )
    if not errors and stderr_text.strip():
        errors.append(
            {
                "kind": "C++ Compile Error",
                "line": None,
                "message": stderr_text.strip(),
                "raw": stderr_text.strip(),
            }
        )
    return errors


def parse_javac_errors(stderr_text: str) -> list[dict]:
    """Parse javac compiler diagnostics into structured error objects."""
    errors: list[dict] = []
    for raw_line in stderr_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        m = JAVAC_DIAG_RE.match(line)
        if m and m.group(2).lower() == "error":
            errors.append(
                {
                    "kind": "Java Compile Error",
                    "line": int(m.group(1)),
                    "message": m.group(3),
                    "raw": line,
                }
            )
    if not errors and stderr_text.strip():
        errors.append(
            {
                "kind": "Java Compile Error",
                "line": None,
                "message": stderr_text.strip(),
                "raw": stderr_text.strip(),
            }
        )
    return errors


def decode_partial(val: str | bytes | None) -> str:
    if val is None:
        return ""
    if isinstance(val, bytes):
        return val.decode("utf-8", errors="replace")
    return val


def make_error_response(
    language: str,
    kind: str,
    message: str,
    summary: str = "",
    raw_output: str = "",
    elapsed_ms: float = 0.0,
    exit_code: int = -1,
    stdout: str = "",
    stderr: str = "",
    stage: str = "toolchain",
):
    return jsonify(
        {
            "language": language,
            "success": False,
            "stage": stage,
            "exit_code": exit_code,
            "ast": "",
            "semantic": "",
            "symbol_table": "",
            "tac": "",
            "stdout": stdout,
            "stderr": stderr,
            "errors": [
                {
                    "kind": kind,
                    "line": None,
                    "message": message,
                    "raw": message,
                }
            ],
            "error_summary": summary or message,
            "error_text": stderr or message,
            "raw_output": raw_output or stderr or message,
            "elapsed_ms": elapsed_ms,
        }
    )


def handle_vertex_compile(code: str, display_name: str):
    compiler_bin = find_compiler_binary()
    if compiler_bin is None:
        return (
            make_error_response(
                language="vertex",
                kind="System Error",
                message="Compiled binary (compiler / compiler.exe) not found in project root. Run 'make' first.",
                summary="Compiler binary not found.",
            ),
            500,
        )

    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".mc", encoding="utf-8", newline="\n", delete=False
        ) as tmp_file:
            tmp_file.write(code)
            tmp_path = tmp_file.name

        t0 = time.perf_counter()
        proc = subprocess.run(
            [str(compiler_bin), tmp_path],
            capture_output=True,
            text=True,
            timeout=10,
            cwd=str(PROJECT_ROOT),
        )
        elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
    except subprocess.TimeoutExpired:
        return (
            make_error_response(
                language="vertex",
                kind="Timeout Error",
                message="Compiler process timed out after 10 seconds.",
                summary="Compilation timed out.",
                elapsed_ms=10000,
                stage="compile",
            ),
            504,
        )
    finally:
        if tmp_path and os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError:
                pass

    stdout_clean = (proc.stdout or "").replace(tmp_path, display_name)
    stderr_clean = (proc.stderr or "").replace(tmp_path, display_name)
    raw_output = stdout_clean + (
        ("\n" if stdout_clean and not stdout_clean.endswith("\n") else "") + stderr_clean
        if stderr_clean
        else ""
    )

    sections = parse_compiler_stdout(stdout_clean)
    errors, error_summary = parse_compiler_errors(stderr_clean)
    success = proc.returncode == 0 and len(errors) == 0

    return jsonify(
        {
            "language": "vertex",
            "success": success,
            "stage": "done" if success else "compile",
            "exit_code": proc.returncode,
            "ast": sections["ast"],
            "semantic": sections["semantic"],
            "symbol_table": sections["symbol_table"],
            "tac": sections["tac"],
            "stdout": stdout_clean,
            "stderr": stderr_clean,
            "errors": errors,
            "error_summary": error_summary,
            "error_text": stderr_clean.strip(),
            "raw_output": raw_output.strip("\r\n"),
            "elapsed_ms": elapsed_ms,
        }
    )


def handle_cpp_compile(code: str):
    gpp_path = find_gpp()
    if gpp_path is None:
        return make_error_response(
            language="cpp",
            kind="Toolchain Error",
            message=(
                "C++ compiler 'g++' is not installed or not found on PATH. "
                "Install g++ (e.g., MSYS2 MinGW-w64 on Windows or 'sudo apt install g++' on Linux) "
                "and restart the server."
            ),
            summary="Missing toolchain: g++ not found.",
            stage="toolchain",
        )

    env = os.environ.copy()
    gpp_bin_dir = str(gpp_path.parent)
    env["PATH"] = gpp_bin_dir + os.pathsep + env.get("PATH", "")

    t0 = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="vertex_cpp_") as tmp_dir:
        tmp_path = Path(tmp_dir)
        src_file = tmp_path / "main.cpp"
        exe_file = tmp_path / ("main.exe" if os.name == "nt" else "main")
        src_file.write_text(code, encoding="utf-8", newline="\n")

        # Step 1: Compile with g++
        try:
            comp = subprocess.run(
                [str(gpp_path), "-std=c++17", "-Wall", "-Wextra", str(src_file), "-o", str(exe_file)],
                capture_output=True,
                text=True,
                timeout=COMPILE_TIMEOUT_SECONDS,
                cwd=tmp_dir,
                env=env,
            )
        except subprocess.TimeoutExpired:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            return make_error_response(
                language="cpp",
                kind="Timeout Error",
                message=f"g++ compilation timed out after {COMPILE_TIMEOUT_SECONDS} seconds.",
                summary="C++ compilation timed out.",
                elapsed_ms=elapsed_ms,
                stage="compile",
            )

        comp_stderr = (comp.stderr or "").replace(str(src_file), "main.cpp")
        comp_stdout = (comp.stdout or "").replace(str(src_file), "main.cpp")

        if comp.returncode != 0:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            errors = parse_gpp_errors(comp_stderr)
            raw = (comp_stdout + ("\n" if comp_stdout and comp_stderr else "") + comp_stderr).strip()
            return jsonify(
                {
                    "language": "cpp",
                    "success": False,
                    "stage": "compile",
                    "exit_code": comp.returncode,
                    "ast": "",
                    "semantic": "",
                    "symbol_table": "",
                    "tac": "",
                    "stdout": "",
                    "stderr": comp_stderr.strip(),
                    "errors": errors,
                    "error_summary": f"g++ compilation failed with exit code {comp.returncode}.",
                    "error_text": comp_stderr.strip(),
                    "raw_output": raw,
                    "elapsed_ms": elapsed_ms,
                }
            )

        # Step 2: Run compiled binary with 5-second timeout
        try:
            run_proc = subprocess.run(
                [str(exe_file)],
                capture_output=True,
                text=True,
                timeout=RUN_TIMEOUT_SECONDS,
                cwd=tmp_dir,
                env=env,
            )
        except subprocess.TimeoutExpired as exc:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            partial_out = decode_partial(exc.stdout)
            partial_err = decode_partial(exc.stderr)
            raw_parts = []
            if partial_out:
                raw_parts.append(f"=== stdout (before timeout) ===\n{partial_out.rstrip()}")
            if partial_err:
                raw_parts.append(f"=== stderr (before timeout) ===\n{partial_err.rstrip()}")
            raw_parts.append(f"Execution timed out after {RUN_TIMEOUT_SECONDS} seconds.")
            return make_error_response(
                language="cpp",
                kind="Timeout Error",
                message=f"C++ program execution timed out after {RUN_TIMEOUT_SECONDS} seconds (possible infinite loop).",
                summary=f"Execution timed out after {RUN_TIMEOUT_SECONDS}s.",
                raw_output="\n\n".join(raw_parts),
                elapsed_ms=elapsed_ms,
                stdout=partial_out,
                stderr=partial_err,
                stage="run",
            )

        elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
        run_stdout = run_proc.stdout or ""
        run_stderr = run_proc.stderr or ""
        success = run_proc.returncode == 0

        raw_parts = []
        if comp_stderr.strip():
            raw_parts.append(f"=== Compiler Warnings ===\n{comp_stderr.strip()}")
        if run_stdout:
            raw_parts.append(f"=== Program Output (stdout) ===\n{run_stdout.rstrip()}")
        if run_stderr:
            raw_parts.append(f"=== Program Errors (stderr) ===\n{run_stderr.rstrip()}")
        if not raw_parts:
            raw_parts.append(f"(program exited with code {run_proc.returncode} and produced no output)")

        errors: list[dict] = []
        if not success:
            errors.append(
                {
                    "kind": "Runtime Error",
                    "line": None,
                    "message": run_stderr.strip() or f"Process exited with non-zero status code {run_proc.returncode}.",
                    "raw": run_stderr.strip() or f"Exit code {run_proc.returncode}",
                }
            )

        return jsonify(
            {
                "language": "cpp",
                "success": success,
                "stage": "done" if success else "run",
                "exit_code": run_proc.returncode,
                "ast": "",
                "semantic": "",
                "symbol_table": "",
                "tac": "",
                "stdout": run_stdout,
                "stderr": run_stderr,
                "warnings": comp_stderr.strip(),
                "errors": errors,
                "error_summary": "" if success else f"Program exited with code {run_proc.returncode}.",
                "error_text": run_stderr.strip(),
                "raw_output": "\n\n".join(raw_parts),
                "elapsed_ms": elapsed_ms,
            }
        )


def handle_java_compile(code: str):
    javac_path = find_jdk_tool("javac")
    if javac_path is None:
        return make_error_response(
            language="java",
            kind="Toolchain Error",
            message=(
                "Java compiler 'javac' is not installed or not found on PATH. "
                "Install a JDK (e.g., OpenJDK / Oracle JDK) and ensure 'javac' and 'java' are on PATH."
            ),
            summary="Missing toolchain: javac not found.",
            stage="toolchain",
        )

    java_path = find_jdk_tool("java")
    if java_path is None:
        return make_error_response(
            language="java",
            kind="Toolchain Error",
            message=(
                "Java runtime 'java' is not installed or not found on PATH. "
                "Install a JDK/JRE and ensure 'java' is on PATH."
            ),
            summary="Missing toolchain: java not found.",
            stage="toolchain",
        )

    t0 = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="vertex_java_") as tmp_dir:
        tmp_path = Path(tmp_dir)
        src_file = tmp_path / "Main.java"
        src_file.write_text(code, encoding="utf-8", newline="\n")

        # Step 1: Compile with javac Main.java
        try:
            comp = subprocess.run(
                [str(javac_path), "Main.java"],
                capture_output=True,
                text=True,
                timeout=COMPILE_TIMEOUT_SECONDS,
                cwd=tmp_dir,
            )
        except subprocess.TimeoutExpired:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            return make_error_response(
                language="java",
                kind="Timeout Error",
                message=f"javac compilation timed out after {COMPILE_TIMEOUT_SECONDS} seconds.",
                summary="Java compilation timed out.",
                elapsed_ms=elapsed_ms,
                stage="compile",
            )

        comp_stderr = (comp.stderr or "").replace(str(src_file), "Main.java")
        comp_stdout = (comp.stdout or "").replace(str(src_file), "Main.java")

        if comp.returncode != 0:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            errors = parse_javac_errors(comp_stderr)
            raw = (comp_stdout + ("\n" if comp_stdout and comp_stderr else "") + comp_stderr).strip()
            return jsonify(
                {
                    "language": "java",
                    "success": False,
                    "stage": "compile",
                    "exit_code": comp.returncode,
                    "ast": "",
                    "semantic": "",
                    "symbol_table": "",
                    "tac": "",
                    "stdout": "",
                    "stderr": comp_stderr.strip(),
                    "errors": errors,
                    "error_summary": f"javac compilation failed with exit code {comp.returncode}.",
                    "error_text": comp_stderr.strip(),
                    "raw_output": raw,
                    "elapsed_ms": elapsed_ms,
                }
            )

        # Step 2: Run with java Main (5-second timeout)
        try:
            run_proc = subprocess.run(
                [str(java_path), "-cp", tmp_dir, "Main"],
                capture_output=True,
                text=True,
                timeout=RUN_TIMEOUT_SECONDS,
                cwd=tmp_dir,
            )
        except subprocess.TimeoutExpired as exc:
            elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
            partial_out = decode_partial(exc.stdout)
            partial_err = decode_partial(exc.stderr)
            raw_parts = []
            if partial_out:
                raw_parts.append(f"=== stdout (before timeout) ===\n{partial_out.rstrip()}")
            if partial_err:
                raw_parts.append(f"=== stderr (before timeout) ===\n{partial_err.rstrip()}")
            raw_parts.append(f"Execution timed out after {RUN_TIMEOUT_SECONDS} seconds.")
            return make_error_response(
                language="java",
                kind="Timeout Error",
                message=f"Java program execution timed out after {RUN_TIMEOUT_SECONDS} seconds (possible infinite loop).",
                summary=f"Execution timed out after {RUN_TIMEOUT_SECONDS}s.",
                raw_output="\n\n".join(raw_parts),
                elapsed_ms=elapsed_ms,
                stdout=partial_out,
                stderr=partial_err,
                stage="run",
            )

        elapsed_ms = round((time.perf_counter() - t0) * 1000, 2)
        run_stdout = run_proc.stdout or ""
        run_stderr = run_proc.stderr or ""
        success = run_proc.returncode == 0

        raw_parts = []
        if comp_stderr.strip():
            raw_parts.append(f"=== Compiler Notes ===\n{comp_stderr.strip()}")
        if run_stdout:
            raw_parts.append(f"=== Program Output (stdout) ===\n{run_stdout.rstrip()}")
        if run_stderr:
            raw_parts.append(f"=== Program Errors (stderr) ===\n{run_stderr.rstrip()}")
        if not raw_parts:
            raw_parts.append(f"(program exited with code {run_proc.returncode} and produced no output)")

        errors: list[dict] = []
        if not success:
            line_match = re.search(r"Main\.java:(\d+)\)", run_stderr)
            err_line = int(line_match.group(1)) if line_match else None
            first_line = run_stderr.strip().splitlines()[0] if run_stderr.strip() else f"Process exited with code {run_proc.returncode}."
            errors.append(
                {
                    "kind": "Runtime Error",
                    "line": err_line,
                    "message": first_line,
                    "raw": run_stderr.strip() or f"Exit code {run_proc.returncode}",
                }
            )

        return jsonify(
            {
                "language": "java",
                "success": success,
                "stage": "done" if success else "run",
                "exit_code": run_proc.returncode,
                "ast": "",
                "semantic": "",
                "symbol_table": "",
                "tac": "",
                "stdout": run_stdout,
                "stderr": run_stderr,
                "warnings": comp_stderr.strip(),
                "errors": errors,
                "error_summary": "" if success else (run_stderr.strip() or f"Program exited with code {run_proc.returncode}."),
                "error_text": run_stderr.strip(),
                "raw_output": "\n\n".join(raw_parts),
                "elapsed_ms": elapsed_ms,
            }
        )


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/samples", methods=["GET"])
def list_samples():
    """List all .mc and .txt sample/test files under examples/ and tests/."""
    samples = []
    for folder_name in ("examples", "tests"):
        base_dir = PROJECT_ROOT / folder_name
        if not base_dir.is_dir():
            continue
        for file_path in sorted(base_dir.rglob("*")):
            if not file_path.is_file():
                continue
            if file_path.suffix.lower() not in (".mc", ".txt"):
                continue
            rel_path = file_path.relative_to(PROJECT_ROOT).as_posix()
            sort_order, category = classify_sample(rel_path)
            try:
                content = file_path.read_text(encoding="utf-8")
            except OSError:
                content = ""
            samples.append(
                {
                    "path": rel_path,
                    "name": file_path.name,
                    "category": category,
                    "sort_order": sort_order,
                    "code": content,
                }
            )

    samples.sort(key=lambda item: (item["sort_order"], item["path"]))
    for item in samples:
        item.pop("sort_order", None)

    return jsonify({"samples": samples})


@app.route("/compile", methods=["POST"])
def compile_code():
    """Dispatch compilation/execution based on selected language mode (vertex, cpp, java)."""
    payload = request.get_json(silent=True) or {}
    language = (payload.get("language") or "vertex").strip().lower()
    code = payload.get("code", "")
    display_name = payload.get("filename") or "editor.mc"

    if language == "vertex":
        return handle_vertex_compile(code, display_name)
    if language == "cpp":
        return handle_cpp_compile(code)
    if language == "java":
        return handle_java_compile(code)

    return (
        make_error_response(
            language=language,
            kind="Invalid Request",
            message=f"Unsupported language mode '{language}'. Supported modes: vertex, cpp, java.",
        ),
        400,
    )


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000, debug=True)
