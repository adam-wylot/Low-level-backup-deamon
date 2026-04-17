
Model 2:50 PM
# SOP Backup Daemon (Real-time Directory Synchronizer)

### 📌 Project Overview
SOP Backup is a robust, interactive Linux background service (daemon-like) written in **C17**. It allows users to continuously monitor a source directory and mirror its changes to one or multiple destination directories in real-time.

This project was built to demonstrate deep knowledge of Linux Systems Programming (POSIX), including process management, asynchronous signal handling, I/O multiplexing, and advanced file system operations.

---

### ✨ Key Features
*   **Real-time Synchronization**: Uses `inotify` API combined with `poll()` to instantly detect file creations, modifications, deletions, and moves.
*   **1-to-N Backup Architecture**: A single source directory can be backed up to multiple destinations simultaneously.
*   **Interactive Command Line**: Built-in shell to add, remove, list, and restore backups dynamically without restarting the main program.
*   **Smart File Handling**: Properly handles regular files, directories, and symlinks (including path relativization). Preserves file metadata (permissions, access/modification times).
*   **Safe Restore Mechanism**: DFS-based directory cleaning and conditional copying (only newer or missing files are restored).
*   **Memory Safety**: Custom dynamic data structures designed from scratch without memory leaks (verified with AddressSanitizer).

---

### 🛠️ Technical Stack & Skills Demonstrated
As a system-level project, this codebase relies heavily on standard C libraries and Linux system calls:

#### Process Management
*   Leveraging `fork()` to spawn independent background worker processes for each backup task.
*   Asynchronous child harvesting using `SIGCHLD` and `waitpid(..., WNOHANG)` to prevent zombie processes.

#### Signal Handling
*   Custom signal handlers (`sigaction`) for graceful shutdowns (`SIGTERM`, `SIGINT`).
*   Explicit signal blocking/unblocking (`sigprocmask`) to create critical sections during sensitive file operations (preventing corruption if interrupted).
*   Robust `EINTR` (Interrupted System Call) handling across all blocking operations (reads, writes, polls).

#### File System APIs
*   File Tree Walk (`nftw()`) for initial deep-copy synchronization and recursive deletions.
*   Low-level file descriptors (`open()`, `read()`, `write()`, `lstat()`, `fchown()`, `futimens()`) for efficient copying.

#### I/O Multiplexing
*   `poll()` used alongside `inotify` file descriptors to implement non-blocking event loops with timeouts.

#### Custom Data Structures
*   Implemented a custom Hash Map (`hashmap.c`) utilizing the `djb2` hashing algorithm.
*   Extended to a complex associative array (`hashmapcomplex.c`) to manage 1-to-many paths and PID mappings.
*   Dynamic string builder (`cstring.c`) for safe memory reallocation during command parsing.

---

### 🏗️ Architecture
*   **Main Process**: Acts as an interactive shell. It parses user input, manages the complex hash map of active backups, and forks child processes.
*   **Child Processes (Workers)**: Each child is responsible for a specific source -> destination pair.
    *   **Phase 1**: Uses `nftw` to perform an initial sync, creating the directory structure and copying existing files.
    *   **Phase 2**: Enters a `poll()` loop, reading `inotify_event` structs. It dynamically adds/removes watches as subdirectories are created or deleted.

---

### 🚀 Usage

#### Build
The project uses a Makefile with strict compilation flags (`-Wall -Wextra -Wshadow`) and AddressSanitizer enabled for development.
```bash
make all
```

#### Run
```bash
./sop-backup
```

#### Available Commands (Interactive Mode)
Once the program is running, you can use the following commands:
*   `add <src> <dest1> [dest2] ...`: Starts backing up `<src>` to one or more `<dest>` locations in the background. (Supports both relative and absolute paths).
*   `end <src> <dest>`: Stops the backup process for a specific pair.
*   `restore <src> <dest>`: Temporarily halts the backup, restores the content from `<dest>` back to `<src>`, and resumes monitoring.
*   `list`: Displays a tree-like list of all active backup tasks.
*   `exit`: Gracefully terminates all child processes, frees memory, and exits the program.

---

### 🛡️ Edge Cases Handled
*   **Inotify Queue Overflow**: Detects `IN_Q_OVERFLOW` and triggers a full fallback synchronization (`nftw`) to ensure no events are lost.
*   **Symlink Resolution**: Automatically detects if a symlink points inside the source directory and relativizes its target to the destination directory to prevent broken links in the backup.
*   **Directory Validations**: Prevents cyclic backups (e.g., destination inside the source directory) and checks for permissions/existence before initializing workers.
*   **Safe Bulk I/O**: Custom `bulk_read` and `bulk_write` wrappers to handle partial reads/writes and system interruptions.
