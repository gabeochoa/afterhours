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
