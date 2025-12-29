# Feature Branch Structure for GitHub

## Overview

This document defines the GitHub branch structure and workflow for developing new MycoBrain capabilities (WiFi Sense, MycoDRONE, etc.).

---

## Branch Hierarchy

```
main (production)
  │
  ├── develop (integration)
  │     │
  │     ├── feature/wifisense-phase0
  │     ├── feature/wifisense-phase1
  │     ├── feature/mycodrone-phase1
  │     ├── feature/mycodrone-phase2
  │     ├── bugfix/wifisense-csi-parsing
  │     └── hotfix/mycodrone-payload-release
  │
  └── release/v1.2.0
```

---

## Branch Types

### 1. Feature Branches

**Naming**: `feature/<feature-name>-<phase>`

**Examples:**
- `feature/wifisense-phase0`
- `feature/wifisense-phase1`
- `feature/mycodrone-phase1`
- `feature/mycodrone-phase2`

**Purpose**: Develop new capabilities

**Workflow:**
1. Create from `develop`
2. Develop feature
3. Create PR to `develop`
4. Merge after review
5. Delete branch

### 2. Bugfix Branches

**Naming**: `bugfix/<bug-description>`

**Examples:**
- `bugfix/wifisense-csi-parsing`
- `bugfix/mycodrone-landing-sequence`
- `bugfix/mdp-telemetry-crc`

**Purpose**: Fix bugs in features

**Workflow:**
1. Create from `develop` (or `feature/*` if bug is in feature branch)
2. Fix bug
3. Create PR to `develop` (or feature branch)
4. Merge after review
5. Delete branch

### 3. Hotfix Branches

**Naming**: `hotfix/<issue-description>`

**Examples:**
- `hotfix/mycodrone-payload-release`
- `hotfix/mindex-api-crash`
- `hotfix/natureos-auth-bypass`

**Purpose**: Fix critical production issues

**Workflow:**
1. Create from `main`
2. Fix issue
3. Create PR to `main` and `develop`
4. Merge to both
5. Tag release
6. Delete branch

### 4. Release Branches

**Naming**: `release/<version>`

**Examples:**
- `release/v1.2.0`
- `release/v1.3.0-wifisense`
- `release/v2.0.0-mycodrone`

**Purpose**: Prepare release

**Workflow:**
1. Create from `develop`
2. Final testing
3. Version bump
4. CHANGELOG update
5. Create PR to `main`
6. Merge and tag
7. Delete branch

---

## Repository Structure

### Recommended Directory Layout

```
mycobrain/
├── firmware/
│   ├── common/              # Shared MDP code
│   ├── side_a/              # Side-A firmware
│   ├── side_b/              # Side-B firmware
│   ├── gateway/             # Gateway firmware
│   └── features/            # Feature-specific firmware
│       ├── wifisense/       # WiFi Sense firmware
│       └── mycodrone/       # MycoDRONE firmware
│
├── mindex/
│   ├── migrations/          # Database migrations
│   ├── mindex_api/          # API code
│   │   ├── routers/         # API routes
│   │   │   ├── wifisense.py
│   │   │   └── drone.py
│   │   └── schemas/         # Pydantic models
│   └── mindex_etl/          # ETL jobs
│
├── natureos/
│   ├── app/                 # Next.js app
│   │   ├── api/             # API routes
│   │   │   ├── wifisense/
│   │   │   └── drone/
│   │   └── natureos/        # Pages
│   └── components/          # React components
│       ├── wifisense/
│       └── drone/
│
├── mas/
│   └── mycosoft_mas/
│       └── agents/          # AI agents
│           ├── wifisense_agent.py
│           └── drone_agent.py
│
└── docs/
    ├── features/            # Feature specifications
    └── Architecture.md
```

---

## Pull Request Workflow

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Feature (new capability)
- [ ] Bug fix
- [ ] Documentation
- [ ] Refactoring

## Related Issue
Closes #<issue-number>

## Testing
- [ ] Unit tests passing
- [ ] Integration tests passing
- [ ] Field tests completed (if applicable)

## Checklist
- [ ] Code follows style guide
- [ ] Self-review completed
- [ ] Comments added for complex code
- [ ] Documentation updated
- [ ] No new warnings
- [ ] Breaking changes documented
```

### PR Labels

- `feature` - New feature
- `bugfix` - Bug fix
- `documentation` - Documentation changes
- `refactoring` - Code refactoring
- `breaking` - Breaking changes
- `wifisense` - WiFi Sense related
- `mycodrone` - MycoDRONE related
- `firmware` - Firmware changes
- `mindex` - MINDEX changes
- `natureos` - NatureOS changes
- `mas` - MAS changes

### PR Review Process

1. **Author**: Create PR, assign reviewers
2. **Reviewers**: Review code, request changes if needed
3. **CI/CD**: Run automated tests
4. **Author**: Address review comments
5. **Reviewers**: Approve PR
6. **Maintainer**: Merge PR

---

## Commit Message Convention

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Code style (formatting)
- `refactor`: Code refactoring
- `test`: Tests
- `chore`: Maintenance

### Examples

```
feat(wifisense): add CSI telemetry support

Add WiFi Sense CSI telemetry structure to MDP protocol.
Includes CSI data format, link ID, and channel information.

Closes #123

fix(mcodrone): fix payload release sequence

Correct payload release sequence to prevent premature release.

Refs #456

docs(features): add WiFi Sense specification

Add comprehensive WiFi Sense capability specification document.
```

---

## Feature Flag Strategy

### Environment Variables

```bash
# WiFi Sense
WIFISENSE_ENABLED=true
WIFISENSE_PHASE=0  # 0, 1, or 2

# MycoDRONE
MYCODRONE_ENABLED=true
MYCODRONE_PHASE=1  # 1, 2, or 3
```

### Code Example

```typescript
// NatureOS: app/api/wifisense/status/route.ts
export async function GET() {
  if (process.env.WIFISENSE_ENABLED !== 'true') {
    return Response.json({ error: 'WiFi Sense not enabled' }, { status: 503 });
  }
  
  // ... implementation
}
```

```python
# MINDEX: mindex_api/routers/wifisense.py
@router.post("/wifisense/ingest")
async def ingest_csi(packet: WiFiSenseTelemetryV1):
    if not os.getenv("WIFISENSE_ENABLED") == "true":
        raise HTTPException(503, "WiFi Sense not enabled")
    
    # ... implementation
```

---

## CI/CD Pipeline

### GitHub Actions Workflow

```yaml
name: Feature Tests

on:
  pull_request:
    branches: [develop, main]
  push:
    branches: [develop, main]

jobs:
  test-firmware:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Test firmware
        run: |
          cd firmware
          # Run firmware tests
  
  test-mindex:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Test MINDEX
        run: |
          cd mindex
          pytest
  
  test-natureos:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Test NatureOS
        run: |
          cd natureos
          pnpm test
```

---

## Release Strategy

### Versioning

- **Major** (X.0.0): Breaking changes
- **Minor** (0.X.0): New features (backward compatible)
- **Patch** (0.0.X): Bug fixes

### Release Schedule

- **Major releases**: Quarterly or as needed
- **Minor releases**: Monthly
- **Patch releases**: As needed

### Release Process

1. Create release branch from `develop`
2. Update version numbers
3. Update CHANGELOG.md
4. Run full test suite
5. Create PR to `main`
6. Merge after approval
7. Tag release: `git tag v1.2.0`
8. Push tags: `git push --tags`
9. Merge `main` back to `develop`
10. Deploy to production

---

## Best Practices

### Branch Management

1. **Keep branches short-lived**: Merge within 1-2 weeks
2. **Rebase before merge**: Keep history clean
3. **Delete merged branches**: Clean up after merge
4. **Use descriptive names**: Clear branch purpose

### Code Quality

1. **Write tests**: Unit + integration tests
2. **Follow style guide**: Consistent code style
3. **Document code**: Comments for complex logic
4. **Review before merge**: At least one approval

### Communication

1. **Update issues**: Link PRs to issues
2. **Notify team**: Tag relevant reviewers
3. **Document decisions**: Architecture decisions in ADRs
4. **Share progress**: Regular status updates

---

## Example Workflow: WiFi Sense Phase 0

### 1. Create Feature Branch

```bash
git checkout develop
git pull origin develop
git checkout -b feature/wifisense-phase0
```

### 2. Develop Feature

```bash
# Make changes
git add .
git commit -m "feat(wifisense): add CSI telemetry structure"
git push origin feature/wifisense-phase0
```

### 3. Create Pull Request

- Title: `feat: WiFi Sense Phase 0 - Occupancy Detection`
- Description: Link to specification document
- Labels: `feature`, `wifisense`, `firmware`, `mindex`, `natureos`
- Reviewers: Assign relevant team members

### 4. Address Review Comments

```bash
# Make changes
git add .
git commit -m "fix(wifisense): address review comments"
git push origin feature/wifisense-phase0
```

### 5. Merge to Develop

- After approval, merge PR
- Delete branch
- Continue development or create release

---

## Resources

- [GitHub Flow](https://guides.github.com/introduction/flow/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Semantic Versioning](https://semver.org/)
- [Feature Specifications](./README.md)

---

**Last Updated**: 2025-01-XX

