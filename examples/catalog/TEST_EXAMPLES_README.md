# Test examples

From the library root, run the standard suite with
`nice -n 10 make -C tests -j2 test`.

Focused catalog targets:

```sh
nice -n 10 make -C examples/catalog/utility/logging_test
nice -n 10 make -C examples/catalog/safety/component_overflow_test
nice -n 10 make -C examples/catalog/safety/component_overflow_test force_test
```

Logging tests exercise the replacement entry points. Component overflow must report a
useful diagnostic rather than corrupt storage; inspect the forced-overflow target’s
expected failure handling before treating its exit status as a regression.
Other small examples cover arena usage, text caching and tag filters under this catalog.

## Example helper

`examples/example_helper.h` holds the repeated scaffolding so an example file
can focus on the behavior it demonstrates:

- Always available: `examples::Checks`, `run_frames`, `run_seconds`, and
  `update_systems<...>()` for small ECS programs.
- Define `AFTER_HOURS_EXAMPLE_UI` for `LogicUI`/`make_logic_ui` and the shared
  `examples::InputAction` vocabulary.
- Also define `AFTER_HOURS_EXAMPLE_GRAPHICS` for RAII graphics init,
  `HeadlessUI`, and one-frame `ImmScreenshot` capture.
- Add `AFTER_HOURS_EXAMPLE_TEST_INPUT` only when an example needs synthetic
  clicks through `HeadlessUI::click_at`.
- Define `AFTER_HOURS_EXAMPLE_E2E` for `E2EHarness` (argument parsing,
  built-in handler registration order, script/dir loading, runner loop).

Migrated so far: `core/simple_system`, `plugins/animation`,
`plugins/e2e_register_command`, `builds/pure_ecs`, `builds/logic_only_ui`, `ui/disclosure`,
`ui/click_without_focus`, and `on_draw_example`.
