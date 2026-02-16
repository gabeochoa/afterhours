# Sound System Plugin

1. **HIGH — Avoid linear scan in `play_if_none_playing` and `play_first_available_match`.**
   Both iterate all matches checking `IsSoundPlaying`. Cache which sounds are playing.

2. **MED — Replace `std::map<string, vector<string>>` in `SoundEmitter` with flat hash maps.**
   `std::map` is a tree with O(log N) lookups and string comparison overhead.

3. **MED — Pre-compute alias names at load time, not on first play.**
   `ensure_alias_names` runs lazily on first play, causing a stutter.

4. **MED — Avoid `std::string` copies in `play_with_alias_or_name`.**
   `const std::string base{name}` copies the C-string.

5. **MED — Cache `SoundLibrary::get()` singleton access in local variable.**
   Multiple calls to `SoundLibrary::get()` in the same function.

6. **MED — Use `string_view` for sound name lookups to avoid allocation.**

7. **MED — Avoid `try/catch` blocks in `play_with_alias_or_name`; use `contains()` check.**
   Exception handling has significant overhead even when not thrown.

8. **MED — Batch `MusicLibrary::update()` to only update actively playing streams.**

9. **LOW — Replace `std::multimap` in `Library` base with a flat hash multimap.**

10. **LOW — Pool-allocate `PlaySoundRequest` components.**

11. **LOW — Avoid `removeComponent<PlaySoundRequest>` after every play; use a "processed" flag.**

12. **LOW — Use `std::string_view` in `Library::load` / `Library::get` to avoid copies.**

13. **LOW — Pre-size `alias_names_by_base` reserve based on expected sound count.**

14. **LOW — Avoid modular arithmetic in round-robin alias selection; use branch-free increment.**

15. **LOW — Cache volume settings to avoid redundant `SetSoundVolume` calls.**

16. **LOW — Use a lock-free queue for sound playback requests from other threads.**

17. **LOW — Avoid `std::to_string` in alias name generation; use `fmt::format_int`.**

18. **LOW — Skip `MusicUpdateSystem` when no music is loaded.**

19. **LOW — Profile `raylib::LoadSound` and consider async loading.**

20. **LOW — Deduplicate sound loads (multiple loads of the same file).**

21. **LOW — Use memory-mapped files for large audio assets.**

22. **LOW — Pre-load commonly used sounds at startup instead of on-demand.**

23. **LOW — Reduce singleton lookup overhead with a cached pointer.**

24. **LOW — Avoid `std::string` concatenation in `ensure_alias_names`; use fixed buffer.**

25. **LOW — Use `constexpr` for default volume values.**

26. **LOW — Profile and tune `default_alias_copies` (4) per game requirements.**

27. **LOW — Add a sound priority system to skip low-priority sounds when many are playing.**

28. **LOW — Use spatial audio culling to skip sounds outside the listener's range.**

29. **LOW — Batch volume updates instead of iterating all sounds per change.**

30. **LOW — Avoid `auto &kv : storage` in volume update; use index-based iteration.**

31. **LOW — Cache the `SoundEmitter` singleton pointer in `SoundPlaybackSystem::once()`.**

32. **LOW — Use `std::move` for `PlaySoundRequest::name` / `prefix` fields.**

33. **LOW — Replace `Policy` enum dispatch with a vtable-free approach (template parameter).**

34. **LOW — Pre-sort sounds by name for faster prefix matching.**

35. **LOW — Consider using audio middleware (FMOD/Wwise) for better performance at scale.**
