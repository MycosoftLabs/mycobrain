# MycoBrain Feature Development Guide

This directory contains specifications and implementation plans for new MycoBrain capabilities.

## Current Features

### 1. WiFi Sense
**Status**: Specification Complete  
**Document**: [WiFiSense-Capability-Spec.md](./WiFiSense-Capability-Spec.md)

WiFi-based presence, motion, and pose sensing using Channel State Information (CSI).

**Phases:**
- Phase 0: Occupancy + Motion (3-6 months)
- Phase 1: Body Silhouette / Coarse Keypoints (6-12 months)
- Phase 2: Dense Pose (12-24 months)

### 2. MycoDRONE
**Status**: Specification Complete  
**Document**: [MycoDRONE-Capability-Spec.md](./MycoDRONE-Capability-Spec.md)

Autonomous quadcopter for deploying and retrieving Mushroom 1 Nodes and SporeBase units.

**Phases:**
- Phase 1: Open-field autonomous deploy (fastest to first test)
- Phase 2: Precision landing + recovery under canopy edges
- Phase 3: Forest-capable navigation

---

## Feature Development Workflow

### 1. Specification Phase

1. Create feature specification document in `docs/features/`
2. Review with team
3. Get approval
4. Create GitHub issue with label `feature`

### 2. Planning Phase

1. Break down into implementation tasks
2. Create GitHub project board
3. Estimate effort
4. Assign tasks

### 3. Implementation Phase

1. Create feature branch: `feature/wifisense` or `feature/mycodrone`
2. Implement following architecture:
   - **Firmware**: Extend MycoBrain firmware
   - **MINDEX**: Database schema + API
   - **NatureOS**: Dashboard widgets + API routes
   - **MAS**: Agents (if applicable)
3. Write tests
4. Update documentation

### 4. Testing Phase

1. Unit tests
2. Integration tests
3. Field tests
4. Performance tests

### 5. Integration Phase

1. Merge to `develop` branch
2. Integration testing
3. Documentation updates
4. Release notes

### 6. Release Phase

1. Merge to `main` branch
2. Tag release
3. Deploy to production
4. Monitor

---

## GitHub Branch Structure

### Branch Naming Convention

```
feature/<feature-name>          # Feature development
bugfix/<bug-name>               # Bug fixes
hotfix/<issue-name>              # Hot fixes
release/<version>                # Release preparation
```

### Example Branches

```
feature/wifisense-phase0
feature/mycodrone-phase1
bugfix/wifisense-csi-parsing
hotfix/mycodrone-payload-release
release/v1.2.0
```

### Branch Protection Rules

- `main`: Requires PR review, CI passing, no force push
- `develop`: Requires CI passing, no force push
- `feature/*`: No restrictions (developer branches)

---

## Implementation Checklist Template

### Firmware

- [ ] MDP protocol extensions (new message types, commands)
- [ ] Telemetry structures
- [ ] Command handlers
- [ ] Hardware interface code
- [ ] Unit tests
- [ ] Integration tests

### MINDEX

- [ ] Database migrations
- [ ] Schema definitions
- [ ] API endpoints
- [ ] Pydantic models
- [ ] Unit tests
- [ ] Integration tests

### NatureOS

- [ ] API routes
- [ ] Dashboard widgets
- [ ] UI components
- [ ] TypeScript types
- [ ] Unit tests
- [ ] E2E tests

### MAS (if applicable)

- [ ] Agent implementation
- [ ] Integration code
- [ ] Unit tests

### Documentation

- [ ] API documentation
- [ ] User guide
- [ ] Developer guide
- [ ] Release notes

---

## Testing Strategy

### Unit Tests

- Test individual components in isolation
- Mock external dependencies
- Achieve >80% code coverage

### Integration Tests

- Test component interactions
- Use test databases/containers
- Verify end-to-end flows

### Field Tests

- Deploy to real hardware
- Test in real-world conditions
- Collect performance metrics

---

## Code Review Guidelines

### PR Requirements

1. **Description**: Clear description of changes
2. **Tests**: All tests passing
3. **Documentation**: Updated documentation
4. **Breaking Changes**: Clearly marked
5. **Migration Guide**: If schema changes

### Review Checklist

- [ ] Code follows style guide
- [ ] Tests are comprehensive
- [ ] Documentation is updated
- [ ] No security issues
- [ ] Performance is acceptable
- [ ] Backward compatibility maintained

---

## Release Process

### Versioning

Follow [Semantic Versioning](https://semver.org/):
- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes

### Release Steps

1. Update version numbers
2. Update CHANGELOG.md
3. Create release branch
4. Run full test suite
5. Create release PR
6. Merge after approval
7. Tag release
8. Deploy to production

---

## Resources

- [MycoBrain Protocol Documentation](../MycoBrainV1-Protocol.md)
- [MINDEX API Documentation](../../MINDEX/mindex/README.md)
- [NatureOS Documentation](../../NATUREOS/NatureOS/README.md)
- [MAS Documentation](../../MAS/mycosoft-mas/README.md)

---

## Questions?

- Open GitHub issue with label `question`
- Contact development team
- Check existing documentation

---

**Last Updated**: 2025-01-XX

