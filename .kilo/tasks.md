# Stellar Engine Task Tracking

## Task Format
Each task entry follows this format:
- [ID] [STATUS] [PRIORITY] [AGENT] - [DESCRIPTION]
  - Failure Details (if applicable): [WHAT IS WRONG] | [LOCATION IN CODE]
  - Attempts: [NUMBER]/2
  - Dependencies: [LIST OF DEPENDENT TASK IDs]
  - Created: [TIMESTAMP]
  - Updated: [TIMESTAMP]

## Status Values
- PENDING: Task not yet started
- IN_PROGRESS: Task currently being worked on
- COMPLETED: Task finished successfully
- FAILED: Task failed after maximum attempts
- CANCELLED: Task no longer needed

## Priority Values
- HIGH: Critical path task
- MEDIUM: Important but not blocking
- LOW: Nice to have or experimental

## Current Tasks


#### Example Task Entry
```markdown
- [T001] [FAILED] [HIGH] [@carmack] - Implement ECS world serialization
  - Failure Details: Component ID registration conflict | include/stellar/ecs/Registry.hpp:45
  - Attempts: 2/2
  - Dependencies: []
  - Created: 2026-04-22T19:00:00-05:00
  - Updated: 2026-04-22T19:15:00-05:00
```