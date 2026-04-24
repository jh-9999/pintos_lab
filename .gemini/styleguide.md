# Gemini Code Assist Style Guide

This repository is a learning-oriented Pintos project. Review comments must
preserve the student's opportunity to find algorithmic, concurrency, scheduler,
memory, and OS behavior issues through their own debugging process.

These instructions override the standard Gemini Code Assist review categories
for this repository. Do not perform general correctness, efficiency,
maintainability, security, or miscellaneous production review except for the
style and typo checks explicitly allowed below.

## Primary Reference

Use `.gemini/CODING_CONVENTIONS_CHECKLIST.md` as the authoritative checklist for style
review. When a changed line violates a checklist rule, cite the matching rule ID
such as `GEN-010`, `IND-010`, `CALL-010`, `DEF-010`, `NAME-010`, `PTR-010`,
`CTRL-010`, `ELSE-010`, `CMT-010`, or `VERIFY-010`.

## Review Scope

Only review changed lines in the pull request. Use surrounding code only to
understand local style. Do not comment on untouched legacy code.

Allowed review topics:

- Formatting and Pintos style convention violations from
  `.gemini/CODING_CONVENTIONS_CHECKLIST.md`.
- Variable, function, macro, enum, or struct naming typos.
- Obvious spelling mistakes in comments, messages, and documentation.
- Accidental unrelated formatting churn in changed lines.

Disallowed review topics:

- Do not point out logic errors.
- Do not mention that a logic error may exist.
- Do not point out scheduler correctness issues.
- Do not point out synchronization, race, deadlock, or priority donation bugs.
- Do not point out MLFQS formula or timing bugs.
- Do not point out memory management, pointer lifetime, or resource cleanup
  bugs unless the issue is purely a naming/style typo.
- Do not suggest algorithm changes.
- Do not suggest production-hardening, performance tuning, security hardening,
  architecture refactors, or maintainability refactors.
- Do not provide implementation hints that would solve Pintos test failures.
- Do not request whole-file reformatting.

## Comment Style

All review comments must be written in Korean. Do not use English except for
identifiers such as function names, variables, rule IDs, or code snippets.

Keep comments short and low severity. Prefer one comment per distinct style or
typo issue. If a finding is not clearly about style or typo, do not comment.

Each style comment should include:

- The matching checklist rule ID from `.gemini/CODING_CONVENTIONS_CHECKLIST.md`.
- The minimal changed line or phrase that violates the rule.
- A concise style-only suggestion.

Do not include broad code review summaries about correctness, performance,
security, or design.