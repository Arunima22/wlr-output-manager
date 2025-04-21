## How to Run

1. Compile using:  
   `gcc -o main main.c -lwayland-client -lm`

2. Run the program:  
   `./main`

3. Commands:
   - `list_outputs`
   - `set_output`
   - `monitor`
   - `exit`

---

### Command Descriptions

#### `list_outputs`
- Lists all outputs and their properties.

---

#### `set_output`
- Changes the properties of an output such as mode, scale, etc.

**Syntax:**  
`set_output <output name> <property name> <value> <property name> <value> ...`

- You can specify **up to 5 properties**.
- **No property** should be repeated.

**Property Options:**

- `mode` — `width,height@refresh` for standard mode.
- `cmode` — `width,height@refresh` for custom mode.
- `scale` — `<value>` for setting the scale factor.
- `transform` — `<value>` for rotating.
- `adaptivesync` — `<value>` for enabling/disabling adaptive sync.

---

#### `monitor`
- Shows a log of requests sent and events received.

**Usage:**

- `monitor` — shows all log entries.
- `monitor single YYYY-MM-DD::HH:MM:SS` — shows all logs at a specific timestamp.
- `monitor period YYYY-MM-DD::HH:MM:SS YYYY-MM-DD::HH:MM:SS` — shows logs between two timestamps.
