# Reader-Writer Simulation

## Overview
This homework implements a simulation of the Reader-Writer problem using POSIX threads (`pthread`) and semaphores (`sem_t`). The program is designed to manage concurrent read and write operations on tables (or files) while ensuring synchronization and avoiding race conditions. Readers can access shared resources simultaneously, whereas writers require exclusive access.

---

## Features
- Multiple readers can read from the files simultaneously.
- When a writer is writing to a file, no readers or other writers can access it.
- If a writer is waiting, no new readers may start reading the file until the writer has completed its update.
- Logs all activities (read and write operations) to a log file (`logfile.txt`).

---

## Files
### Source Code
- `databaseSim.c`: The implementation of the Reader-Writer simulation.

### Output
- `logfile.txt`: Logs of the program.
- `tbl<i>.txt`: Tables.

---

## How to Compile
Use the following command to compile the program:
```bash
gcc databaseSim.c -o databaseSim
```

---

## How to Run
Execute the program with the following syntax:
```bash
./databaseSim <Number_of_thread> <Number_of_tables> <ActivityFile Name>
```

### Example:
```bash
./databaseSim 10 2 activity.txt
```
This example creates 10 threads and 2 tables and reads activities from `activities.txt`.

---

## Input File Format
The activity file should follow the format:
```
<operation_time> <thread_id> <table_id> <operation_type> <duration> <written_value>
```
- `operation_time`: Time (in seconds) after which the operation starts.
- `thread_id`: Identifier of the thread performing the operation.
- `table_id`: ID of the table to operate on.
- `operation_type`: Either `read` or `write`.
- `duration`: Duration (in seconds) of the operation.
- `written_value`: Value to be written (applicable for write operations only).

Example:
```
5 1 1 read 3 “ ”
6 2 1 read 2 “ “
7 3 1 write 10 “ INFORMATION 1 “
9 4 1 read 2 “ “
```

---

## Output Details
- `logfile.txt`: Logs all read and write operations in the following format:
  - **Reader Logs**
    - `Reader <thread_id> started reading <table>`
    - `Reader <thread_id> finished reading <table>`
  - **Writer Logs**
    - `Writer <thread_id> waiting to write to <table>`
    - `Writer <thread_id> started writing to <table>`
    - `Writer <thread_id> finished writing to <table>`

- `tbl<i>.txt`: Stores values written by writers to the corresponding table.

---

## Code Breakdown
### Key Components
- **Semaphores:**
  - `mutex[i]`: Ensures mutual exclusion for reader count updates.
  - `write_lock[i]`: Ensures exclusive access for writers.
- **Activities:**
  - Activities are parsed from the input file and stored in an array.
- **Threads:**
  - Each thread processes activities assigned to it.

### Core Functions
- `create_file(const char *filename)`: Initializes table files.
- `perform_read(int thread_id, int table_id, int duration)`: Handles read operations.
- `perform_write(int thread_id, int table_id, int duration, const char *value)`: Handles write operations.
- `parse_activity_file(const char *filename)`: Parses activities from the input file.
- `thread_function(void *arg)`: Executes activities for a given thread.

---

## Sample Run

### Activity File

```
5 1 1 read 3 “ ”
6 2 1 read 2 “ “
7 3 1 write 10 “ INFORMATION 1 “
9 4 1 read 2 “ “
```

### Commands

```bash
gcc databaseSim.c -o databaseSim
```
```bash
./databaseSim 10 2 activity.txt
```

### Log File

```
Reader 1 started reading tbl1.txt
Reader 2 started reading tbl1.txt
Writer 3 waiting to write to tbl1.txt
Reader 2 finished reading tbl1.txt
Reader 1 finished reading tbl1.txt
Writer 3 started writing to tbl1.txt
Writer 3 finished writing to tbl1.txt
Reader 4 started reading tbl1.txt
Reader 4 finished reading tbl1.txt

```

### tbl1.txt File
```
“ INFORMATION 1 “

```
### tb2.txt File
```
```

## Author
Tolgahan Keleş
2311051801

