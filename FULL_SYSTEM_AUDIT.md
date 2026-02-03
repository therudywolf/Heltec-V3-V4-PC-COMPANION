# NOCTURNE OS: CRITICAL UI RECOVERY & REWRITE

## 1. THE AUDIT (READ PHASE)

@DisplayEngine.cpp @SceneManager.cpp @config.h
**Context:** The current UI is BROKEN.

1.  **Header:** The text is physically CLIPPED on the left (X < 2) and the right side is EMPTY.
2.  **Main Scene:** No visual separation between Temp and Load %. The RAM bar at the bottom is ugly/misaligned.
3.  **General:** Elements are drifting off-screen.

**TASK:** You must read the current rendering code and REWRITE IT completely based on the strictly defined coordinate system below. Do NOT preserve the old logic if it violates these rules.

---

## 2. THE REWRITE RULES (WRITE PHASE)

### A. HEADER REPAIR (`DisplayEngine::drawGlobalHeader`)

The header is the "Face" of the system. It must be perfect.

- **Coordinate Rule:** NEVER draw text at X=0. Minimum X = 4.
- **Action:**
  1.  Draw the Scene Name at **X = 4** (Padding).
  2.  **Right Side Injection:** You MUST draw a system status label aligned to the right.
      - Calculate width of text "**NOCT:ON**" or "**SYS:V3**".
      - Draw it at `X = 128 - width - 4`.
  3.  **The Line:** Draw the bottom separator line at Y = 12.

### B. MAIN SCENE REPAIR (`SceneManager::drawMain`)

The current layout is "Mush". We need "Grid".

- **Vertical Split:** Draw a `drawDottedVLine` at X=64 (Center).
- **CPU Block (Left, X=0..63):**
  - **Temp:** Draw Big Value (e.g., "55°") at **Y=30** (Centered in top half).
  - **Separator:** Draw a solid `drawHLine` from X=10 to X=54 at **Y=36**.
  - **Load:** Draw a **Segmented Bar** (not a text %) at **Y=40**, H=6.
    - Overlay tiny text "25%" centered on the bar or right above it.
- **GPU Block (Right, X=64..127):**
  - Mirror the CPU logic exactly.
- **RAM Block (Bottom, Y=48..63):**
  - **Separator:** Draw `drawDottedHLine` at Y=48.
  - **Label:** "RAM" at X=4, Y=58 (Tiny Font).
  - **Visual:** Draw a "Tech Frame" (drawFrame) at X=30, Y=50, W=94, H=10.
  - **Fill:** Fill the box based on usage.
  - **Text:** Draw "8.2GB" inside the box (inverted color) or next to it.

### C. VISUAL TWEAKS

- **Fonts:** Ensure `u8g2_font_profont10_mr` is used for all labels (CPU, GPU, RAM). Use `u8g2_font_helvB10_tr` ONLY for the Temperature numbers.
- **Icons:** If weather icons are failing, implement the fallback geometric drawing (Circle for Sun, Box for Cloud) immediately.

---

## 3. EXECUTION COMMAND

Refactor `DisplayEngine.cpp` (Header) and `SceneManager.cpp` (Main, Fans, MB) NOW.
**Constraint:** Use `snprintf` for all strings. Do not use dynamic String objects. Hardcode the Y-coordinates provided above.

Show me the fixed code for `drawGlobalHeader` and `drawMain` first.
