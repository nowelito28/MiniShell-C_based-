
# 🐚 Custom C Shell - `vlite.1.0`

A lightweight UNIX shell implementation written in C for educational purposes. It supports basic command execution, I/O redirection, background processing, variable substitution, and custom built-in commands.

---

## 📜 Description

This project is a simple command-line shell (`vlite.1.0`) implemented in C for Linux-based systems. It replicates many core functionalities of common UNIX shells like `sh` or `bash`, with constraints that encourage understanding of system calls and low-level process control.

The shell:
- Reads input from standard input or script files
- Parses and executes commands
- Manages environment variables
- Handles redirections (`<`, `>`)
- Supports background execution (`&`)
- Implements several built-in commands (e.g., `cd`, `ifok`, `ifnot`)
- Avoids using high-level exec functions like `execvp()` or `system()`

---

## 🚀 Features

- [x] Command execution via `fork()` and `execve()`
- [x] Input/output redirection using `<` and `>`
- [x] Background execution with `&`, with proper redirection to `/dev/null`
- [x] Variable substitution (e.g., `$var`) and assignment (`var=value`)
- [x] Script mode (e.g., `./shell < script.txt`)
- [x] Built-in `cd` to change working directory
- [x] Built-in `ifok` and `ifnot` conditional commands
- [x] `result` variable stores the return status of the last command
- [x] Optional: `HERE{` multi-line input blocks (here-documents)
- [x] Optional: globbing support (e.g., `*.c`)

---

## 🛠️ Installation

Clone the repository, then compile to generate a binary object file and link to execute the shell with:

```bash
gcc -c -Wall -Wshadow -Wvla -g shell.c
gcc -o shell shell.o
```

---

## 🧪 Usage

Start an interactive shell:

```bash
./shell
```

Run a script file:

```bash
./shell < script.txt
```

## 🔧 Syntax and Examples

- Basic command:
  ```
  ls -l /tmp
  ```

- Redirection:
  ```
  cat < input.txt > output.txt
  ```

- Background execution:
  ```
  sleep 5 &
  ```

- Variable assignment and substitution:
  ```bash
  cmd=ls
  $cmd /home
  ```

- Built-in `cd`:
  ```
  cd /etc
  ```

- Conditional execution:
  ```bash
  test -e /tmp
  ifok ls -l /tmp
  ifnot echo "Directory /tmp does not exist"
  ```

- Error handling for undefined variables:
  ```
  echo $patata
  # error: var patata does not exist
  ```

---

## 📁 File Structure

```bash
.
├── shell.c         # Main shell source code
├── shell.pdf       # Project description and specifications
├── README.md       # This file
```

---

## 🔒 License

This project is distributed under the MIT License. See `LICENSE` for more details.

---

## 👨‍🎓 OS learning Purpose

This project is designed as a practical assignment to deepen the understanding of:

- Process management (`fork`, `execve`, `waitpid`)
- File descriptors and I/O redirection
- Tokenization and string handling in C
- Shell parsing and command evaluation
