```markdown
# posix-cosmo-pi Development Patterns

> Auto-generated skill from repository analysis

## Overview
This skill teaches you the core development patterns, coding conventions, and CI workflows for the `posix-cosmo-pi` TypeScript project. You'll learn how to structure code, follow commit conventions, update CI workflows, and write tests in line with established practices.

## Coding Conventions

### File Naming
- Use **camelCase** for file names.
  - Example: `posixUtils.ts`, `fileSystemHelpers.ts`

### Import Style
- Use **relative imports** for internal modules.
  ```typescript
  import { doSomething } from './utils/doSomething'
  ```

### Export Style
- Use **named exports**.
  ```typescript
  // Good
  export function parseInput() { ... }
  export const PI = 3.14

  // Avoid default exports
  ```

### Commit Messages
- Follow **Conventional Commits** format.
- Common prefixes: `ci`, `feat`
  - Example: `feat: add support for custom shell environments`
  - Example: `ci: update shellcheck rules in CI workflow`

## Workflows

### ci-workflow-update
**Trigger:** When you need to adjust CI behavior, such as modifying build steps, test invocation, or shellcheck rules.  
**Command:** `/update-ci`

1. Open `.github/workflows/posix-cosmo.yml`.
2. Edit the file to update build, test, or shellcheck steps as needed.
   - For example, to add a new test step:
     ```yaml
     - name: Run additional tests
       run: npm run test:extra
     ```
   - To update shellcheck configuration:
     ```yaml
     - name: ShellCheck scripts
       run: shellcheck ./scripts/*.sh
     ```
3. Commit your changes with a descriptive message, e.g.:
   ```
   ci: update test invocation to include extra tests
   ```
4. Push your changes and create a pull request if required.

## Testing Patterns

- Test files use the pattern `*.test.*` (e.g., `mathUtils.test.ts`).
- The testing framework is **unknown**; check existing test files for structure.
- Example test file:
  ```typescript
  import { add } from './mathUtils'

  test('adds two numbers', () => {
    expect(add(2, 3)).toBe(5)
  })
  ```

## Commands

| Command    | Purpose                                      |
|------------|----------------------------------------------|
| /update-ci | Update CI workflow steps and configuration   |
```
