# Accessibility VoiceOver Checklist

Run this checklist on macOS before publishing a beta or stable release. Record the tester, date,
macOS version, hardware, LaMusica version, Git commit, and whether the build was signed and
notarized.

## Preconditions

- Run `ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure`.
- Enable VoiceOver in System Settings > Accessibility > VoiceOver.
- Enable Full Keyboard Access in System Settings > Accessibility > Keyboard.
- Prepare one project with a timeline clip, a mixer strip, a piano-roll note, and at least one
  plugin control or plugin-placeholder control.
- Use keyboard navigation only unless a step explicitly asks for pointer confirmation.

## VoiceOver Surface Pass

For each surface, confirm VoiceOver announces a useful role, name, current value when applicable,
and available action.

| Surface | Expected announcement evidence |
| --- | --- |
| Transport play/stop | Button or toggle, localized name, playing/stopped state |
| Record/arm/monitor controls | Toggle role, armed/on/off value text |
| Timeline clip | List item or button role, clip name, bar/beat span |
| Time ruler or playhead | Position text matching the visible bar/beat |
| Mixer fader | Slider role, dB value text from the shared formatter |
| Pan control | Slider role, `Center`, `L<n>`, or `R<n>` value text |
| Meter | Read-only meter role, peak dB and clip state |
| Piano-roll note | Note name or pitch, start position, duration |
| Drum pad or step cell | Toggle/button role, row/column or pad name |
| Browser tree | Tree role, selected item, expandable state when present |
| Inspector fields | Labeled controls with editable values |
| Plugin chooser/control | Button/slider/toggle role and non-empty plugin/control name |
| Export dialog | Modal focus trap, labeled controls, confirm/cancel actions |
| Welcome/templates | Template names, descriptions, recent-project actions |
| Guided tour | Skippable controls and current region name |

## Keyboard-Only Workflows

Complete each workflow with VoiceOver still enabled and without synthetic mouse events.

- Start and stop transport.
- Arm a track and toggle monitoring.
- Select a timeline clip and perform one edit command.
- Change a mixer fader value and verify the audible/session value changes.
- Open the plugin chooser or plugin control surface and inspect one control.
- Open the export path, review the controls, cancel, then reopen and confirm export.
- Open the welcome window, choose a template, and return focus to the main shell.
- Restart and skip the guided tour.

## Motion And Contrast

- Enable Reduce Motion and confirm playhead/meter animation no longer runs continuously; value text
  must still update at the reduced cadence.
- Enable Increase Contrast and confirm the high-contrast palette is active, focus rings are visible,
  and primary foreground/background pairs remain legible.

## Failure Policy

A release fails this gate if any interactive control is announced as an unlabeled generic group, if a
primary workflow cannot be completed by keyboard with VoiceOver enabled, if focus escapes a modal
dialog, or if Reduce Motion / Increase Contrast preferences are ignored.
