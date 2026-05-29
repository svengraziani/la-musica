# Accessibility VoiceOver Evidence

Complete this file for each beta or stable macOS release and attach it to the release notes.

## Build

- Release version:
- Git commit:
- Artifact name:
- Signing identity:
- Notarization request id:
- Stapled artifact validated: yes/no

## Test Environment

- Tester:
- Date:
- macOS version:
- Hardware:
- VoiceOver enabled: yes/no
- Full Keyboard Access enabled: yes/no
- Reduce Motion tested: yes/no
- Increase Contrast tested: yes/no

## Automated Gate

- Command:
  `ctest --preset release-universal -R lamusica_daw_accessibility_audit --output-on-failure`
- Result:
- Log or CI run:

## Surface Results

| Surface | Role/name/value evidence | Pass/fail | Notes |
| --- | --- | --- | --- |
| Transport play/stop |  |  |  |
| Record/arm/monitor controls |  |  |  |
| Timeline clip |  |  |  |
| Time ruler or playhead |  |  |  |
| Mixer fader |  |  |  |
| Pan control |  |  |  |
| Meter |  |  |  |
| Piano-roll note |  |  |  |
| Drum pad or step cell |  |  |  |
| Browser tree |  |  |  |
| Inspector fields |  |  |  |
| Plugin chooser/control |  |  |  |
| Export dialog |  |  |  |
| Welcome/templates |  |  |  |
| Guided tour |  |  |  |

## Keyboard-Only Workflows

| Workflow | Completed without mouse | VoiceOver evidence | Notes |
| --- | --- | --- | --- |
| Start and stop transport |  |  |  |
| Arm a track and toggle monitoring |  |  |  |
| Select and edit a timeline clip |  |  |  |
| Change a mixer fader value |  |  |  |
| Inspect a plugin control |  |  |  |
| Cancel and confirm export |  |  |  |
| Choose an onboarding template |  |  |  |
| Restart and skip guided tour |  |  |  |

## Failures And Follow-Up

- Blocking failures:
- Non-blocking observations:
- Follow-up issue links:
- Release approved by:
