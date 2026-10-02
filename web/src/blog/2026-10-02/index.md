title: Seeing it the way Beta 4b did
summary: v1.0.4.0 gives the Hidden his 110 field of view, puts weapons back where Beta 4b drew them, marks team chat, shows the kill feed on widescreen and fixes a stuck crouch view.

# Seeing it the way Beta 4b did

2026-10-02

[v1.0.4.0](https://github.com/phit/hidden-2013/releases/tag/v1.0.4.0) is mostly about what you
see: the field of view, your weapon, the chat and the kill feed, each checked against Beta 4b side
by side. It also fixes several things players reported.

## View and weapons

- **The Hidden's 110 field of view:** Beta 4b gave the Hidden a wider view than the marines' 90.
  Ours asked for it but lost it on the first weapon switch. He now has it all round, along with
  the wider knife and hands and the faster mouse turn that came with it in Beta 4b.
- **Weapons where Beta 4b drew them:** guns and the knife were drawn too close and too low, with
  the Hidden's hands half out of frame. They now line up with Beta 4b at 4:3 and on widescreen.
- **Scoped aiming:** a marine looking through the FN2000's scope turns at Beta 4b's slower speed
  again; it was two and a half times too fast.

## Gameplay

- **Carrying guns:** the Hidden can pick up a dead marine's gun with +use and throw it, as in
  Beta 4b.
- **Letting go of props inside the Hidden:** a barrel or body dropped while it overlaps him no
  longer shoves him around; it collides with him again once he's clear of it.
- **Crouch view:** crouching again just after standing up could leave your view at standing height
  while you were still crouched; it was reported when crouching to pounce. Fixed.
- **Crouching marines walking backwards** move at a third of their speed, as in Beta 4b, instead
  of almost full speed.
- **Fall damage** starts at Beta 4b's height: marines no longer get hurt dropping from ledges that
  were safe in Beta 4b.
- **After dying** you start spectating as soon as the death animation ends, instead of looking at a
  black screen for three seconds.

## HUD and chat

- **Team chat is marked "(Team)"**, so you can tell it from public chat. Beta 4b never marked it;
  servers can turn the label off with `hdn_teamchat_prefix 0`.
- **Joins, leaves and team changes** show in Beta 4b's pale chat text instead of green and yellow
  console lines.
- **The kill feed** shows on widescreen resolutions, where it used to be pushed off the screen, and
  uses Beta 4b's weapon icons. Pistol and shotgun kills used to show no icon at all, and everything
  else a skull.

## The site

This site moved to [www.hidden-rebuild.com](https://www.hidden-rebuild.com/), and links to it now
show a preview card in Discord and elsewhere. The default message of the day links it too.

Servers only accept players on the same version, so update both when you get this one; the launcher
does it for you.
