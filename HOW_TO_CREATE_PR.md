# How to Create the Pull Request

## Option 1: Using GitHub Web Interface (Recommended)

1. **Push your branch** (if not already pushed):
   ```bash
   git push origin feature/high-compression-rate-algorithm-nguyen
   ```

2. **Go to GitHub**:
   - Navigate to: https://github.com/Tuanminhngo/Maptek-SEP-WeEight
   - You should see a yellow banner saying "feature/high-compression-rate-algorithm-nguyen had recent pushes"
   - Click the green **"Compare & pull request"** button

3. **Fill in the PR details**:
   - **Title**: `feat: High-compression rate algorithms with streaming support`
   - **Description**: Copy the contents from `PR_DESCRIPTION.md` (in this folder)
   - **Base branch**: `main`
   - **Compare branch**: `feature/high-compression-rate-algorithm-nguyen`

4. **Review the changes**:
   - Scroll down to see the file changes
   - Verify that all your commits are included

5. **Create the PR**:
   - Click the green **"Create pull request"** button

## Option 2: Using GitHub CLI (if you install it)

1. **Install GitHub CLI** (optional):
   ```bash
   brew install gh
   ```

2. **Login**:
   ```bash
   gh auth login
   ```

3. **Create PR**:
   ```bash
   gh pr create --title "feat: High-compression rate algorithms with streaming support" \
                --body-file PR_DESCRIPTION.md \
                --base main \
                --head feature/high-compression-rate-algorithm-nguyen
   ```

## Option 3: Direct URL

1. **Go directly to create PR page**:
   ```
   https://github.com/Tuanminhngo/Maptek-SEP-WeEight/compare/main...feature/high-compression-rate-algorithm-nguyen
   ```

2. **Fill in details** using `PR_DESCRIPTION.md`

3. **Click "Create pull request"**

## After Creating the PR

1. **Add reviewers** (if required by your project)
2. **Add labels** like `enhancement`, `algorithm`, `performance`
3. **Link to issues** if this PR closes any issues
4. **Wait for CI/CD** checks to pass (if configured)
5. **Respond to review comments** from team members

## Troubleshooting

### "Nothing to compare" error
- Make sure your branch is pushed to GitHub
- Verify the branch name is correct
- Check that there are actually commits different from main

### Need to update your branch
```bash
# If main has moved forward
git checkout feature/high-compression-rate-algorithm-nguyen
git pull origin main
git push origin feature/high-compression-rate-algorithm-nguyen
```

### Want to add more commits before PR
```bash
# Make your changes
git add .
git commit -m "your message"
git push origin feature/high-compression-rate-algorithm-nguyen
# The PR will automatically update with new commits
```
