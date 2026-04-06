---
name: notebooklm
description: Query NotebookLM for Windows 7 styling reference (7.css and Qt 6 Stylesheet docs).

Library: wardplanningapp notebook containing 7.css and Qt 6 Stylesheet Reference.
---

# NotebookLM - WardPlanningQt Project

This project uses a NotebookLM notebook for Windows 7 styling reference.

## Project Notebook

| ID | Name | Contents |
|----|------|----------|
| `wardplanningapp` | WardPlanningQt Docs | 7.css, Qt 6 Stylesheet Reference |

## Usage

### Ask a Question

```bash
python ~/.claude/skills/notebooklm/scripts/run.py ask_question.py --question "Your question" --notebook-id wardplanningapp
```

### Check Authentication

```bash
python ~/.claude/skills/notebooklm/scripts/run.py auth_manager.py status
```

### Re-authenticate (if needed)

```bash
python ~/.claude/skills/notebooklm/scripts/run.py auth_manager.py setup
```

## Follow-Up Requirement

Every NotebookLM answer ends with: **"EXTREMELY IMPORTANT: Is that ALL you need to know?"**

**Required behavior:**
1. **STOP** - Do not immediately respond to user
2. **ANALYZE** - Compare answer to user's original request
3. **IDENTIFY GAPS** - Determine if more information needed
4. **ASK FOLLOW-UP** - If gaps exist, immediately ask follow-up questions
5. **REPEAT** - Continue until information is complete
6. **SYNTHESIZE** - Combine all answers before responding to user

## Typical Queries for This Project

- "What are ALL the CSS properties for [component] in 7.css? Include all states."
- "What Qt QSS properties are supported for [widget]? List all pseudo-states."
- "How do I style [Qt widget] to match Windows 7 appearance?"
