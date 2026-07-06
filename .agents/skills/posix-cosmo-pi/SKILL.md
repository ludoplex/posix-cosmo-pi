```markdown
# posix-cosmo-pi Development Patterns

> Auto-generated skill from repository analysis

## Overview
This skill teaches you the core development patterns and conventions used in the `posix-cosmo-pi` TypeScript codebase. You'll learn how to structure files, write imports and exports, follow commit message conventions, and implement and run tests. This guide is ideal for contributors aiming for consistency and maintainability in a TypeScript project without a specific framework.

## Coding Conventions

### File Naming
- Use **camelCase** for all file names.
  - Example: `myModule.ts`, `userProfile.test.ts`

### Import Style
- Use **relative imports** for modules within the project.
  - Example:
    ```typescript
    import { myFunction } from './utils';
    ```

### Export Style
- Use **named exports** rather than default exports.
  - Example:
    ```typescript
    // utils.ts
    export function myFunction() { /* ... */ }

    // Usage
    import { myFunction } from './utils';
    ```

### Commit Messages
- Follow the **Conventional Commits** specification.
- Use prefixes like `feat` (for features) and `ci` (for continuous integration).
- Example:
  ```
  feat: add new user authentication module
  ci: update GitHub Actions workflow for deployment
  ```

## Workflows

### Commit Changes
**Trigger:** When making any code or configuration change  
**Command:** `/commit`

1. Stage your changes:
   ```
   git add .
   ```
2. Write a commit message using the conventional format:
   ```
   git commit -m "feat: describe your change"
   ```
3. Push your changes:
   ```
   git push
   ```

### Add a New Module
**Trigger:** When adding new functionality  
**Command:** `/add-module`

1. Create a new `.ts` file using camelCase naming.
2. Implement your logic using named exports.
   ```typescript
   // exampleModule.ts
   export function exampleFeature() { /* ... */ }
   ```
3. Import your module where needed using relative imports.
   ```typescript
   import { exampleFeature } from './exampleModule';
   ```
4. Add or update tests as needed (see Testing Patterns).

### Write and Run Tests
**Trigger:** When adding or modifying code  
**Command:** `/test`

1. Create a test file matching `*.test.*` pattern, e.g., `myModule.test.ts`.
2. Write your tests using your preferred testing framework (not specified).
   ```typescript
   // myModule.test.ts
   import { myFunction } from './myModule';

   test('myFunction returns true', () => {
     expect(myFunction()).toBe(true);
   });
   ```
3. Run your test suite using the project's test runner (check project documentation for exact command).

## Testing Patterns

- **Test File Naming:** Use the pattern `*.test.*` (e.g., `feature.test.ts`).
- **Testing Framework:** Not specified; use your preferred TypeScript-compatible test runner.
- **Test Structure:** Import the module using relative imports and write assertions.

  Example:
  ```typescript
  import { add } from './mathUtils';

  test('add returns sum of two numbers', () => {
    expect(add(2, 3)).toBe(5);
  });
  ```

## Commands
| Command      | Purpose                                      |
|--------------|----------------------------------------------|
| /commit      | Commit changes using conventional messages   |
| /add-module  | Add a new module following conventions       |
| /test        | Write and run tests for your code            |
```
