# Contributing
Want to hack on XDAG? Awesome! Here are instructions to get you started.
They are not perfect yet. Please let us know what feels wrong or incomplete.

XDAG is an Open Source project and we welcome contributions of all sorts.
There are many ways to help, from reporting issues, contributing code, and
helping us improve our community.

## Topics

- [Security Issues](#security-issues)
- [Community Guidelines](#community-guidelines)
- [Reporting Issues](#reporting-issues)
- [Implementation Design](#implementation-design)
- [Community Improvement](#community-improvement)
- [Translations](#translations)
- [Contribution Guidelines](#contribution-guidelines)
- [Helping in other ways](#helping-in-other-ways)

## Security Issues

The xdag protocol and its implementations are still in heavy development. This means that there may be problems in our protocols, or there may be mistakes in our implementations. And many people are already running nodes on their machines. So we take security vulnerabilities very seriously. If you discover a security issue, please bring it to our attention right away!

If you find a vulnerability that may affect live deployments, please send your report privately to communitymanager@xdag.io, please DO NOT file a public issue.

If the issue is a protocol weakness or something not yet deployed, just discuss it openly.

## Community Guidelines

We want to keep the XDAG community awesome, growing and collaborative. We need your help to keep it that way. To help with this we've come up with some general guidelines for the community as a whole:

- Be nice: Be courteous, respectful and polite to fellow community members: no regional, racial, gender, or other abuse will be tolerated. We like nice people way better than mean ones!

- Encourage diversity and participation: Make everyone in our community feel welcome, regardless of their background and the extent of their contributions, and do everything possible to encourage participation in our community.

- Keep it legal: Basically, don't get anybody in trouble. Share only content that you own, do not share private or sensitive information, and don't break laws.

- Stay on topic: Make sure that you are posting to the correct channel and avoid off-topic discussions. Remember when you update an issue or respond to an email you are potentially sending to a large number of people. Please consider this before you update. Also remember that nobody likes spam.

## Reporting Issues

If you find bugs, mistakes, inconsistencies in the XDAG project's code or
documents, please let us know by filing an issue at the appropriate issue
tracker (we use multiple repositories). No issue is too small.


The main issues for bug reporting are as follows:  
- [xdag/issues](https://github.com/XDagger/xdag/issues) - Issues related to xdag.  
- [DaggerGpuMiner/issues](https://github.com/XDagger/DaggerGpuMiner/issues) - Issues related to DaggerGpuMiner.  
- [QtXdagWallet/issues](https://github.com/XDagger/QtXdagWallet/issues) - Issues related to QtXdagWallet.  
- [explorer/issues](https://github.com/XDagger/explorer/issues) - Issues related to block explorer.  

The [xdag](https://github.com/XDagger/xdag) issues use a template that will guide you through the process of reporting a bug. We will be adding this kind of issue template to other repositories as bug reports become more common.

## Implementation Design

When considering design proposals for implementations, we are looking for:

- A description of the problem this design proposal solves
- Discussion of the tradeoffs involved
- Discussion of the proposed solution

## Community Improvement

The XDAG community requires maintenance of various "public infrastructure" resources. These include documentation, github repositories and more. There is also helping new users with questions, spreading the word about XDAG, and so on. We will be planning and running conferences. Please get in touch if you would like to help out.

## Translations

This community moves very fast, and documentation swiftly gets out of date. For now, we are encouraging would-be translators to hold off from translating large repositories. If you would like to add a translation, please open an issue and ask the leads for a given repository before filing a PR, so that we do not waste efforts.

If anyone has any issues understanding the English documentation, please let us know! If you would like to do so privately, please email @Sofar. We are very sensitive to language issues, and do not want to turn anyone away from hacking because of their language.

## Contribution Guidelines
#### Discuss big changes as Issues first

Significant improvements should be documented as GitHub issues before anybody starts to code. This gives other contributors a chance to point you in the right direction, give feedback on the design, and maybe point out if related work is under way.

Please take a moment to check whether an issue already exists. If it does, it never hurts to add a quick "+1" or "I have this problem too". This helps prioritize the most common problems and requests.

### Pull Requests always welcome

We are always thrilled to receive pull requests, and do our best to process them as quickly as possible. Not sure if that typo is worth a pull request? Do it! We will appreciate it.

We're also trying very hard to keep XDAG focused. This means that we might decide against incorporating a new feature. However, there might be a way to implement that feature on top of (or below) XDAG.

If your pull request is not accepted on the first try, don't be discouraged! If there's a problem with the implementation, hopefully you received feedback on what to improve.

### Git

We use a simple git branching model:

- `master` must always work
- `develop` is the branch for development  
- create feature-branches to merge into `develop`
- all commits must pass testing so that git bisect is easy to run

Just stay current with `develop` (rebase).

### Commit messages

Commit messages must start with a short subject line, followed by an optional,
more detailed explanatory text which is separated from the summary by an empty
line.

### Code

Write clean code. Universally formatted code promotes ease of writing, reading, and maintenance.

### Documentation

Update documentation when creating or modifying features. Test your documentation changes for clarity, concision, and correctness, as well as a clean documentation build.

### Pull Requests

Pull requests descriptions should be as clear as possible. Err on the side of overly specific and include a reference to all related issues. If the pull request is meant to close an issue please use the Github keyword conventions of [closes, fixes, or resolves]( https://help.github.com/articles/closing-issues-via-commit-messages/). If the pull request only completes part of an issue use the [connects keywords]( https://github.com/waffleio/waffle.io/wiki/FAQs#prs-connect-keywords). This helps our tools properly link issues to pull requests. 

### Code Review

We take code quality seriously; we must make sure the code remains correct. We do code review on all changesets. Discuss any comments, then make modifications and push additional commits to your feature branch. Be sure to post a comment after pushing. The new commits will show up in the pull request automatically, but the reviewers will not be notified unless you comment.

### Merge Approval

We use `Thank you for your efforts.` in comments on the code review to indicate acceptance. A change **requires** `Thank you for your efforts.` from the maintainers of each component affected. If you know whom it may be, ping them. 

## Helping in other ways

To be continue.
