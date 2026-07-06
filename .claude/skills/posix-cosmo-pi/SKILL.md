```markdown
# posix-cosmo-pi Development Patterns

> Auto-generated skill from repository analysis

## Overview
This skill covers the core development patterns and conventions used in the `posix-cosmo-pi` TypeScript codebase. It includes file naming, import/export styles, commit message guidelines, and testing patterns. By following these patterns, contributors can ensure consistency and maintainability in the project.

## Coding Conventions

### File Naming
- Use **camelCase** for all file names.
  - Example: `userProfile.ts`, `dataFetcher.ts`

### Import Style
- Use **relative imports** for modules within the project.
  - Example:
    ```typescript
    import { fetchData } from './dataFetcher';
    ```

### Export Style
- Use **named exports** for all modules.
  - Example:
    ```typescript
    // In dataFetcher.ts
    export function fetchData() { /* ... */ }

    // In another file
    import { fetchData } from './dataFetcher';
    ```

### Commit Messages
- Follow the **Conventional Commits** specification.
- Use the `feat` prefix for new features.
- Keep commit messages around 90 characters.
  - Example:
    ```
    feat: add user authentication middleware for secure API endpoints
    ```

## Workflows

### Feature Development
**Trigger:** When adding a new feature or module  
**Command:** `/feature-development`

1. Create a new TypeScript file using camelCase naming.
2. Implement your feature using named exports.
3. Use relative imports to include any dependencies.
4. Write corresponding tests in a `.test.ts` file.
5. Commit your changes with a `feat:` prefix and a descriptive message.

### Testing
**Trigger:** When validating code changes  
**Command:** `/run-tests`

1. Locate or create a test file matching the pattern `*.test.ts`.
2. Write tests for your new or updated code.
3. Run the test suite using your preferred test runner (framework not specified).
4. Ensure all tests pass before committing.

## Testing Patterns

- Test files follow the `*.test.ts` naming convention.
- Place tests alongside the modules they test or in a dedicated test directory.
- Example test file:
  ```typescript
  // userProfile.test.ts
  import { getUserProfile } from './userProfile';

  describe('getUserProfile', () => {
    it('returns correct user data', () => {
      // test implementation
    });
  });
  ```

## Commands
| Command              | Purpose                                   |
|----------------------|-------------------------------------------|
| /feature-development | Start a new feature using project patterns|
| /run-tests           | Run the test suite                        |
```