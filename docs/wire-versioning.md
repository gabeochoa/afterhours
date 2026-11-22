# Wire-versioning policy

For games with networked or recorded state. afterhours ships no
transport; the game owns its wire schema. This policy keeps
independently built peers compatible.

## Two versions, two owners

- **Library protocol version** — the transport contract. It moves only
  when the transport itself changes, never for game schema changes.
- **Game wire label** (e.g. `protocol_hash("mygame-v3")`) — the schema
  identity. Bump it for any message-layout, enum-order, or semantic
  change. Never reuse a message type value.

A peer matching neither value gets a `Bye("version mismatch")`. Log and
report the distinct reason — unsupported library protocol,
unsupported game build, malformed message, match full — not one shared
text for all of them.

## Strict by default

Decoders require the exact expected type and an exhausted reader, so
adding a field is a breaking change, not an append. That holds until a
message's decoders and tests stop requiring exhaustion — decide per
message, and until then rebuild all peers together. Enum order is wire
order: keep the value mapping beside the enum and never insert
mid-enum without bumping the label.
