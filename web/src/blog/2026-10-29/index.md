title: Taunts, safe round ends and the weighting
summary: v1.0.5.0 gives the Hidden his taunt menu back, makes the end of a round safe again, counts damage towards who plays the Hidden next and brings back Beta 4b's round clean-up and damage report.

# Taunts, safe round ends and the weighting

2026-10-29

[v1.0.5.0](https://github.com/phit/hidden-2013/releases/tag/v1.0.5.0) gathers up what players
reported after the last release: most of it about the end of a round and who gets to be the Hidden
next. Each fix was checked against Beta 4b itself.

## Gameplay

- **The Hidden's taunts:** the Hidden lost his taunt menu. It's back, with his own background and
  his eight taunts, from "Behind You" to "You're Next".
- **The radio menu** looks like Beta 4b's again: white labels on dark boxes, a lighter box under the
  mouse, no orange outlines, and centred on widescreen.
- **Safe after the round:** once a round is over, nothing hurts you. Players were already safe from
  each other, but a falling prop, a fall or the world could still kill you while waiting for the
  next round. Beta 4b blocked all of it.
- **A clean start:** a new round clears the last round's bullet holes and blood, puts the map's
  small props back and stops leftover sounds, as Beta 4b did.

## Picking the Hidden

- **Damage counts again:** with weighted selection (the default), the damage you do to the other
  team raises your chance of being the next Hidden, and hurting your own team lowers it. It had
  stopped counting, so whoever killed the Hidden was picked almost every time.
- **The damage report:** when you die, or when the round ends, the console lists the damage you did
  and took that round and your weighting, as in Beta 4b.

## Servers

- **`hdn_weightreport`** puts the weighting in chat (off by default, as in Beta 4b). `1` tells
  each marine their chance of being the next Hidden when the round ends, the way the WeighteR
  plugin did: "You have a 71 in 158 (45%) chance to be hidden". `2` shows everyone's chance when
  the next Hidden is picked, and `3` both.

This release plays with v1.0.4.0 and later, so players and servers can update at their own pace;
the launcher does it for you.
