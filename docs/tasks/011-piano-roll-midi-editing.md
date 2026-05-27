# 011 Piano Roll And MIDI Editing

## Objective

Build a complete piano roll and MIDI event editing surface.

## Dependencies

- 010 MIDI Core.

## Deliverables

- Piano roll grid, keyboard, note drawing, selection, move, resize, split, mute, velocity editing, and audition.
- Controller lanes for velocity, CC, pitch bend, aftertouch, and automation-linked data.
- Scale highlighting, chord labels, ghost notes, fold mode, and drum-note naming.
- Event list editor for precise values.
- UI and command tests for MIDI edits.

## Acceptance Gates

- All MIDI edits use undoable commands.
- Dense MIDI clips remain responsive.
- Controller lanes render and edit independently without corrupting notes.

## Notes For Agents

Keep piano roll editing usable with keyboard shortcuts as well as pointer input.
