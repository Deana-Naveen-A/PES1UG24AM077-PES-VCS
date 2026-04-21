
# PES-VCS Lab Report

**Name:** Deana Naveen A

**SRN:** PESXUG24AM077  

## Phase 1: Object Storage

### Screenshot 1A: test_objects output
<img width="1175" height="190" alt="Screenshot 2026-04-21 123520" src="https://github.com/user-attachments/assets/25351ad6-99bc-4646-b19d-6f93e3e57660" />

### Screenshot 1B: Sharded object store
<img width="876" height="99" alt="Screenshot 2026-04-21 123606" src="https://github.com/user-attachments/assets/3a99b9bc-35c7-4c18-86be-d1e03720146c" />

## Phase 2: Tree Objects

### Screenshot 2A: test_tree output
<img width="688" height="159" alt="Screenshot 2026-04-21 124939" src="https://github.com/user-attachments/assets/39766ce6-21a8-490a-bf9e-a335b39b1f1c" />


### Screenshot 2B: Raw tree object (xxd)
<img width="1490" height="99" alt="Screenshot 2026-04-21 125236" src="https://github.com/user-attachments/assets/60226593-bd44-4e66-a54e-0655793f2f7b" />


## Phase 3: Index (Staging Area)

### Screenshot 3A: pes init → pes add → pes status
<img width="1078" height="495" alt="Screenshot 2026-04-21 130656" src="https://github.com/user-attachments/assets/968f5f53-81f9-4304-9ba8-358881e877ac" />


### Screenshot 3B: cat .pes/index
<img width="954" height="80" alt="Screenshot 2026-04-21 130742" src="https://github.com/user-attachments/assets/09825c7a-4b61-4638-8351-8777efcb86c6" />


## Phase 4: Commits and History

### Screenshot 4A: pes log output
<img width="865" height="419" alt="Screenshot 2026-04-21 131526" src="https://github.com/user-attachments/assets/27105d78-2cb8-41cf-a7ad-df65a0870d58" />


### Screenshot 4B: find .pes -type f | sort
<img width="875" height="302" alt="Screenshot 2026-04-21 131558" src="https://github.com/user-attachments/assets/a80da2f0-cc2f-4285-8605-7505e9e807d2" />


### Screenshot 4C: HEAD and branch ref
<img width="872" height="122" alt="Screenshot 2026-04-21 131615" src="https://github.com/user-attachments/assets/82f51f20-9de4-4449-995a-dc17714ac076" />


### Integration Test
<img width="950" height="693" alt="Screenshot 2026-04-21 131722" src="https://github.com/user-attachments/assets/d1ce0d0c-b173-4391-9141-42ab8c90919e" />
<img width="867" height="774" alt="Screenshot 2026-04-21 131753" src="https://github.com/user-attachments/assets/bf035e5f-b2f6-488e-b96d-6924afb79efa" />
<img width="901" height="356" alt="Screenshot 2026-04-21 131809" src="https://github.com/user-attachments/assets/92e9982a-8491-4cc0-94bf-078c13ea6f7c" />




## Analysis Questions

### Q5.1 — Implementing checkout
To implement `pes checkout <branch>`, HEAD must be updated to contain `ref: refs/heads/<branch>` and the working directory must be updated to match the target branch's tree. This requires reading the target commit, walking its tree recursively, creating/updating files that exist in the target tree, and deleting files that exist in the current tree but not the target. The complexity comes from handling subdirectories and ensuring no data loss for modified files.

### Q5.2 — Detecting dirty working directory
For every file that differs between the current and target branch trees, check the index entry for that file. Compare the working directory file's mtime and size against the stored index metadata. If they differ, the file has been modified since staging — checkout must refuse. This works entirely using index metadata and the object store without re-hashing files.

### Q5.3 — Detached HEAD
In detached HEAD state, new commits are created normally but no branch file is updated. If you switch away, those commits become unreachable from any branch. To recover them, note the commit hash from `pes log` while still in detached HEAD, then create a new branch by writing that hash into `.pes/refs/heads/<new-branch>`.

### Q6.1 — Garbage Collection Algorithm
Use a mark-and-sweep approach: start from all branch refs, follow commit parent pointers marking each commit reachable, then for each commit mark its tree and all blobs/subtrees reachable. Use a hash set for O(1) lookup. Then scan all objects in `.pes/objects/` and delete any not in the reachable set. For 100,000 commits and 50 branches, expect to visit roughly 1.1 million objects (commits + trees + blobs).

### Q6.2 — GC Race Condition
Race condition: GC builds its reachable set (object X not yet reachable) → concurrent commit writes X but hasn't updated HEAD → GC deletes X → commit updates HEAD referencing X → repository corrupted. Git avoids this with a grace period — objects newer than 2 weeks are never deleted, giving in-progress operations time to complete.
EOF
