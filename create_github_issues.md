# GitHub Issues Creation Guide

This guide will help you create the GitHub issues for the MCP library update project.

## Option 1: Manual Creation via GitHub Web Interface

1. Go to your repository: https://github.com/helynranta/cpp-mcp/issues
2. Click "New Issue"
3. Copy the title and description from each issue in `MCP_UPDATE_PLAN.md`
4. Add appropriate labels:
   - **Priority:** `priority: high`, `priority: medium`, `priority: low`
   - **Type:** `enhancement`, `security`, `documentation`, `testing`
   - **Phase:** `phase-1-core`, `phase-2-features`, `phase-3-advanced`, `phase-4-qa`, `phase-5-docs`
5. Assign to developers/agents as needed
6. Set milestones if desired

## Option 2: Using GitHub CLI (gh)

If you have GitHub CLI installed, you can use this script:

```bash
# Issue #1
gh issue create \
  --title "Update MCP Protocol Version to 2025-03-26" \
  --body "See MCP_UPDATE_PLAN.md - Issue #1" \
  --label "priority: high,enhancement,phase-1-core"

# Issue #2
gh issue create \
  --title "Add Streamable HTTP Transport as Primary Transport Mechanism" \
  --body "See MCP_UPDATE_PLAN.md - Issue #2" \
  --label "priority: high,enhancement,phase-1-core"

# Issue #3
gh issue create \
  --title "Upgrade OAuth to 2.1 with PKCE and Remove Implicit Flow" \
  --body "See MCP_UPDATE_PLAN.md - Issue #3" \
  --label "priority: high,security,enhancement,phase-1-core"

# Issue #4
gh issue create \
  --title "Implement Session Management and State Recovery" \
  --body "See MCP_UPDATE_PLAN.md - Issue #4" \
  --label "priority: medium,enhancement,phase-2-features"

# Issue #5
gh issue create \
  --title "Add Tool Metadata Annotations for Security and UI" \
  --body "See MCP_UPDATE_PLAN.md - Issue #5" \
  --label "priority: medium,enhancement,phase-2-features"

# Issue #6
gh issue create \
  --title "Implement Dynamic Progress Notifications with Text Messages" \
  --body "See MCP_UPDATE_PLAN.md - Issue #6" \
  --label "priority: low,enhancement,phase-2-features"

# Issue #7
gh issue create \
  --title "Add Native Audio Stream Support" \
  --body "See MCP_UPDATE_PLAN.md - Issue #7" \
  --label "priority: low,enhancement,phase-3-advanced"

# Issue #8
gh issue create \
  --title "Implement Parameter Auto-completion Suggestions" \
  --body "See MCP_UPDATE_PLAN.md - Issue #8" \
  --label "priority: low,enhancement,phase-2-features"

# Issue #9
gh issue create \
  --title "Enforce HTTPS and Implement Security Best Practices" \
  --body "See MCP_UPDATE_PLAN.md - Issue #9" \
  --label "priority: high,security,phase-1-core"

# Issue #10
gh issue create \
  --title "Implement Multi-user Support with Role Definitions" \
  --body "See MCP_UPDATE_PLAN.md - Issue #10" \
  --label "priority: medium,enhancement,phase-3-advanced"

# Issue #11
gh issue create \
  --title "Update Test Suite for 2025-03-26 Specification" \
  --body "See MCP_UPDATE_PLAN.md - Issue #11" \
  --label "priority: high,testing,phase-4-qa"

# Issue #12
gh issue create \
  --title "Update All Documentation for 2025-03-26 Specification" \
  --body "See MCP_UPDATE_PLAN.md - Issue #12" \
  --label "priority: medium,documentation,phase-5-docs"

# Issue #13
gh issue create \
  --title "Clean Up Deprecated Features from Old Specification" \
  --body "See MCP_UPDATE_PLAN.md - Issue #13" \
  --label "priority: low,cleanup,phase-4-qa"

# Issue #14
gh issue create \
  --title "Performance Testing and Optimization for New Features" \
  --body "See MCP_UPDATE_PLAN.md - Issue #14" \
  --label "priority: medium,testing,phase-4-qa"

# Issue #15
gh issue create \
  --title "Validate Compatibility with GitHub Copilot and Claude Code" \
  --body "See MCP_UPDATE_PLAN.md - Issue #15" \
  --label "priority: high,testing,phase-4-qa"
```

## Option 3: Using GitHub API with Python

Create a file `create_issues.py`:

```python
import requests
import os

# Configuration
GITHUB_TOKEN = os.environ.get('GITHUB_TOKEN')
REPO_OWNER = 'helynranta'
REPO_NAME = 'cpp-mcp'

API_URL = f'https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/issues'

headers = {
    'Authorization': f'token {GITHUB_TOKEN}',
    'Accept': 'application/vnd.github.v3+json'
}

# Issue definitions (extracted from MCP_UPDATE_PLAN.md)
issues = [
    {
        'title': 'Update MCP Protocol Version to 2025-03-26',
        'body': 'See MCP_UPDATE_PLAN.md - Issue #1',
        'labels': ['priority: high', 'enhancement', 'phase-1-core']
    },
    # Add remaining issues...
]

# Create issues
for issue in issues:
    response = requests.post(API_URL, json=issue, headers=headers)
    if response.status_code == 201:
        print(f"Created issue: {issue['title']}")
    else:
        print(f"Failed to create issue: {issue['title']}")
        print(f"Error: {response.text}")
```

## Recommended Labels to Create First

Before creating issues, create these labels in your repository:

### Priority Labels
- `priority: high` (Red, #d73a4a)
- `priority: medium` (Orange, #ff9800)
- `priority: low` (Green, #4caf50)

### Type Labels
- `enhancement` (Blue, #2196f3)
- `security` (Red, #f44336)
- `documentation` (Teal, #009688)
- `testing` (Purple, #9c27b0)
- `cleanup` (Gray, #9e9e9e)

### Phase Labels
- `phase-1-core` (Dark Blue, #1565c0)
- `phase-2-features` (Light Blue, #42a5f5)
- `phase-3-advanced` (Cyan, #00acc1)
- `phase-4-qa` (Amber, #ffa726)
- `phase-5-docs` (Lime, #cddc39)

## Issue Dependencies

When creating issues, note these dependencies in the issue description:

- **Issue #2** depends on **Issue #1**
- **Issue #3** depends on **Issue #1**
- **Issue #4** depends on **Issue #2**
- **Issue #5-9** depend on **Issue #1**
- **Issue #10** depends on **Issue #3, #4**
- **Issue #11** depends on all implementation issues
- **Issue #12** depends on all implementation issues
- **Issue #13** depends on **Issue #2, #3, #6**
- **Issue #14** depends on all implementation issues
- **Issue #15** depends on all implementation issues

## Creating a Parent Issue (Umbrella Issue)

Create a main tracking issue:

**Title:** MCP Library Update: 2024-11-05 → 2025-03-26

**Description:**
```markdown
# MCP Library Specification Update

This is the umbrella issue tracking the update of cpp-mcp from MCP spec 2024-11-05 to 2025-03-26.

## Overview
- Current Version: 2024-11-05
- Target Version: 2025-03-26
- Goal: Ensure compatibility with GitHub Copilot and Claude Code

## Sub-Issues

### Phase 1: Core Updates
- [ ] #[issue-1] Update MCP Protocol Version
- [ ] #[issue-2] Streamable HTTP Transport
- [ ] #[issue-3] OAuth 2.1 with PKCE
- [ ] #[issue-9] HTTPS Enforcement

### Phase 2: Feature Enhancements
- [ ] #[issue-4] Session Management
- [ ] #[issue-5] Tool Metadata
- [ ] #[issue-6] Progress Notifications
- [ ] #[issue-8] Parameter Completion

### Phase 3: Advanced Features
- [ ] #[issue-7] Audio Support
- [ ] #[issue-10] Multi-user Support

### Phase 4: Quality Assurance
- [ ] #[issue-11] Test Suite Update
- [ ] #[issue-13] Remove Deprecated Features
- [ ] #[issue-14] Performance Testing
- [ ] #[issue-15] Compatibility Testing

### Phase 5: Documentation
- [ ] #[issue-12] Documentation Update

## Resources
- See `MCP_UPDATE_PLAN.md` for detailed planning
- [MCP Specification 2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26)
```

## Next Steps

1. Review the `MCP_UPDATE_PLAN.md` file
2. Choose your preferred method to create issues
3. Create the parent umbrella issue first
4. Create all sub-issues
5. Link sub-issues to the parent issue
6. Assign issues to developers/agents
7. Begin implementation following the phases

## Tips for Working with Multiple Agents

- Assign each issue to a specific agent
- Use issue comments for communication
- Reference related issues using `#issue-number`
- Update the parent issue checklist as sub-issues are completed
- Use project boards to track progress visually
- Schedule regular sync meetings for complex dependencies
