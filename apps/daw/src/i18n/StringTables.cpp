#include "i18n/StringTables.hpp"

#include <array>
#include <string>

namespace lamusica::daw::i18n {
namespace {

constexpr std::string_view enTable = R"(language: en
countries: US GB
"LaMusica" = "LaMusica"
"Main DAW window" = "Main DAW window"
"Transport" = "Transport"
"Playback and loop controls" = "Playback and loop controls"
"Play" = "Play"
"Start playback" = "Start playback"
"Stop" = "Stop"
"Stop playback" = "Stop playback"
"Loop" = "Loop"
"Toggle loop playback" = "Toggle loop playback"
"On" = "On"
"Off" = "Off"
"Playhead" = "Playhead"
"Browser" = "Browser"
"Project media and library browser" = "Project media and library browser"
"Timeline" = "Timeline"
"Arrangement timeline" = "Arrangement timeline"
"Generated drums clip" = "Generated drums clip"
"drums clip" = "drums clip"
"Starts at" = "Starts at"
"Timeline clip" = "Timeline clip"
"Clip Launcher" = "Clip Launcher"
"Scene launch grid" = "Scene launch grid"
"Scene A" = "Scene A"
"Launch scene A" = "Launch scene A"
"Drum pattern slot" = "Drum pattern slot"
"Queued" = "Queued"
"Clip launcher slot" = "Clip launcher slot"
"Inspector" = "Inspector"
"Selected clip and track properties" = "Selected clip and track properties"
"Mixer" = "Mixer"
"Track levels, pan, meters, and routing" = "Track levels, pan, meters, and routing"
"Master fader" = "Master fader"
"Master output volume" = "Master output volume"
"Master pan" = "Master pan"
"Master pan position" = "Master pan position"
"Master meter" = "Master meter"
"Master peak level" = "Master peak level"
"Record" = "Record"
"Arm track" = "Arm track"
"Input monitoring" = "Input monitoring"
"Piano Roll" = "Piano Roll"
"MIDI note C3" = "MIDI note C3"
"Piano-roll note" = "Piano-roll note"
"Plugin controls" = "Plugin controls"
"Synth cutoff" = "Synth cutoff"
"Plugin parameter" = "Plugin parameter"
"Export dialog" = "Export dialog"
"Output format" = "Output format"
"Confirm export" = "Confirm export"
"Cancel export" = "Cancel export"
"Stopped" = "Stopped"
"File" = "File"
"Edit" = "Edit"
"View" = "View"
"Audio" = "Audio"
"MIDI" = "MIDI"
"Tools" = "Tools"
"Window" = "Window"
"Help" = "Help"
"Privacy" = "Privacy"
"status.noProjectOpen" = "No project open"
"status.noSelection" = "No selection"
"status.frames" = "frames"
"status.on" = "on"
"status.off" = "off"
"status.ready" = "Ready"
"status.yes" = "yes"
"status.no" = "no"
"status.editable" = "editable"
"status.media" = "media"
"status.ok" = "ok"
"status.missing" = "missing"
"status.tracks" = "Tracks"
"status.clips" = "Clips"
"status.timelinePianoRoll" = "Timeline / Piano Roll"
"status.sections" = "Sections"
"status.midiNotes" = "MIDI notes"
"status.bass" = "Bass"
"status.semitonesShort" = "st"
"status.starterDevices" = "Starter devices"
"status.automationLanes" = "Automation lanes"
"status.recordedTakes" = "Recorded takes"
"status.imports" = "Imports"
"status.generatedRouting" = "Generated Drums -> Master | Generated Bass -> Master"
"status.lastExport" = "Last export"
"status.peak" = "peak"
"status.packageMix" = "Package mix"
"status.stems" = "stems"
"privacy.shareDiagnostics" = "Share Diagnostics"
"privacy.keepPrivate" = "Keep Private"
"privacy.defaultEndpoint" = "default HTTPS endpoint"
"privacy.diagnostics" = "Diagnostics"
"privacy.sharingCrashReports" = "sharing crash reports"
"privacy.localCrashLogsOnly" = "private, local crash logs only"
"privacy.telemetry" = "telemetry"
"privacy.endpoint" = "endpoint"
"privacy.disclosure" = "Sends only scrubbed crash metadata, OS version, app version, commit, and signal. No audio, MIDI, project content, file paths, project names, or usernames."
"privacy.firstRunPrompt" = "Share scrubbed crash diagnostics with the LaMusica project? Dismiss or choose Keep Private to keep reports on this Mac."
"onboarding.template.empty.name" = "Empty"
"onboarding.template.empty.description" = "A silent project with a master output."
"onboarding.template.basicMultitrack.name" = "Basic Multitrack"
"onboarding.template.basicMultitrack.description" = "Three audio tracks routed to master."
"onboarding.template.drumSynth.name" = "Drum + Synth"
"onboarding.template.drumSynth.description" = "Instrument tracks with MIDI and automation."
"onboarding.template.podcastVoice.name" = "Podcast / Voice"
"onboarding.template.podcastVoice.description" = "Host and guest tracks through a voice bus."
"onboarding.help.userManual" = "LaMusica User Manual"
"onboarding.help.showWelcome" = "Show Welcome Window"
"onboarding.help.restartTour" = "Restart Guided Tour"
"onboarding.help.keyboardShortcuts" = "Keyboard Shortcuts"
"onboarding.tour.transport.title" = "Transport"
"onboarding.tour.transport.body" = "Start and stop playback, set the loop, and watch the playhead."
"onboarding.tour.browser.title" = "Browser"
"onboarding.tour.browser.body" = "Find project media and reusable sounds."
"onboarding.tour.timeline.title" = "Timeline"
"onboarding.tour.timeline.body" = "Arrange clips and automation over time."
"onboarding.tour.inspector.title" = "Inspector"
"onboarding.tour.inspector.body" = "Edit the selected clip, track, and routing details."
"onboarding.tour.mixer.title" = "Mixer"
"onboarding.tour.mixer.body" = "Balance tracks, sends, meters, and master output."
"onboarding.welcome.title" = "Welcome"
"onboarding.welcome.description" = "Choose a project template or reopen a recent project."
"onboarding.welcome.projectTemplates" = "Project Templates"
"onboarding.welcome.recentProjects" = "Recent projects"
"onboarding.welcome.openProject" = "Open Project"
"onboarding.welcome.openRecent.help" = "Open the most recent LaMusica project."
"onboarding.chooseTemplateOrRecent" = "Choose a template or open a recent project."
"onboarding.welcome.openRecent" = "Open Most Recent"
"onboarding.tour.skip" = "Skip Tour"
"onboarding.help.keyboardShortcuts.body" = "Space: Play/Stop; Command+O: Open; Command+?: Help"
"onboarding.help.userManual.fallback" = "The bundled user manual is installed with LaMusica."
)";

constexpr std::string_view esTable = R"(language: es
countries: ES MX
"LaMusica" = "LaMusica"
"Main DAW window" = "Ventana principal de LaMusica"
"Transport" = "Transporte"
"Playback and loop controls" = "Controles de reproducción y bucle"
"Play" = "Reproducir"
"Start playback" = "Iniciar reproducción"
"Stop" = "Detener"
"Stop playback" = "Detener reproducción"
"Loop" = "Bucle"
"Toggle loop playback" = "Activar o desactivar la reproducción en bucle"
"On" = "Activado"
"Off" = "Desactivado"
"Playhead" = "Cabezal de reproducción"
"Browser" = "Explorador"
"Project media and library browser" = "Explorador de medios del proyecto y biblioteca"
"Timeline" = "Línea de tiempo"
"Arrangement timeline" = "Línea de tiempo del arreglo"
"Generated drums clip" = "Clip de batería generado"
"drums clip" = "clip de batería"
"Starts at" = "Comienza en"
"Timeline clip" = "Clip de la línea de tiempo"
"Clip Launcher" = "Lanzador de clips"
"Scene launch grid" = "Cuadrícula de lanzamiento de escenas"
"Scene A" = "Escena A"
"Launch scene A" = "Lanzar escena A"
"Drum pattern slot" = "Ranura de patrón de batería"
"Queued" = "En cola"
"Clip launcher slot" = "Ranura del lanzador de clips"
"Inspector" = "Inspector"
"Selected clip and track properties" = "Propiedades del clip y la pista seleccionados"
"Mixer" = "Mezclador"
"Track levels, pan, meters, and routing" = "Niveles de pista, paneo, medidores y ruteo"
"Master fader" = "Fader maestro"
"Master output volume" = "Volumen de salida maestro"
"Master pan" = "Paneo maestro"
"Master pan position" = "Posición de paneo maestro"
"Master meter" = "Medidor maestro"
"Master peak level" = "Nivel pico maestro"
"Record" = "Grabar"
"Arm track" = "Armar pista"
"Input monitoring" = "Monitoreo de entrada"
"Piano Roll" = "Piano roll"
"MIDI note C3" = "Nota MIDI Do3"
"Piano-roll note" = "Nota del piano roll"
"Plugin controls" = "Controles de plugin"
"Synth cutoff" = "Corte del sintetizador"
"Plugin parameter" = "Parámetro de plugin"
"Export dialog" = "Diálogo de exportación"
"Output format" = "Formato de salida"
"Confirm export" = "Confirmar exportación"
"Cancel export" = "Cancelar exportación"
"Stopped" = "Detenido"
"File" = "Archivo"
"Edit" = "Edición"
"View" = "Vista"
"Audio" = "Audio"
"MIDI" = "MIDI"
"Tools" = "Herramientas"
"Window" = "Ventana"
"Help" = "Ayuda"
"Privacy" = "Privacidad"
"status.noProjectOpen" = "No hay proyecto abierto"
"status.noSelection" = "Sin selección"
"status.frames" = "fotogramas"
"status.on" = "activado"
"status.off" = "desactivado"
"status.ready" = "Listo"
"status.yes" = "sí"
"status.no" = "no"
"status.editable" = "se puede editar"
"status.media" = "medios"
"status.ok" = "correcto"
"status.missing" = "faltante"
"status.tracks" = "Pistas"
"status.clips" = "Clips de audio"
"status.timelinePianoRoll" = "Línea de tiempo / Piano roll"
"status.sections" = "Secciones"
"status.midiNotes" = "Notas MIDI"
"status.bass" = "Bajo"
"status.semitonesShort" = "semitonos"
"status.starterDevices" = "Dispositivos iniciales"
"status.automationLanes" = "Carriles de automatización"
"status.recordedTakes" = "Tomas grabadas"
"status.imports" = "Importaciones"
"status.generatedRouting" = "Batería generada -> Master | Bajo generado -> Master"
"status.lastExport" = "Última exportación"
"status.peak" = "pico"
"status.packageMix" = "Mezcla del paquete"
"status.stems" = "pistas separadas"
"privacy.shareDiagnostics" = "Compartir diagnósticos"
"privacy.keepPrivate" = "Mantener privado"
"privacy.defaultEndpoint" = "endpoint HTTPS predeterminado"
"privacy.diagnostics" = "Diagnósticos"
"privacy.sharingCrashReports" = "compartiendo informes de fallo"
"privacy.localCrashLogsOnly" = "privado, solo registros locales de fallos"
"privacy.telemetry" = "telemetría"
"privacy.endpoint" = "destino"
"privacy.disclosure" = "Envía solo metadatos de fallos depurados, versión del sistema, versión de la app, commit y señal. No audio, MIDI, contenido del proyecto, rutas de archivos, nombres de proyecto ni nombres de usuario."
"privacy.firstRunPrompt" = "¿Compartir diagnósticos de fallos depurados con el proyecto LaMusica? Cierra el diálogo o elige Mantener privado para conservar los informes en este Mac."
"onboarding.template.empty.name" = "Vacío"
"onboarding.template.empty.description" = "Un proyecto silencioso con salida maestra."
"onboarding.template.basicMultitrack.name" = "Multipista básico"
"onboarding.template.basicMultitrack.description" = "Tres pistas de audio ruteadas al master."
"onboarding.template.drumSynth.name" = "Batería + Sintetizador"
"onboarding.template.drumSynth.description" = "Pistas de instrumento con MIDI y automatización."
"onboarding.template.podcastVoice.name" = "Podcast / Voz"
"onboarding.template.podcastVoice.description" = "Pistas de anfitrión e invitado por un bus de voz."
"onboarding.help.userManual" = "Manual de usuario de LaMusica"
"onboarding.help.showWelcome" = "Mostrar ventana de bienvenida"
"onboarding.help.restartTour" = "Reiniciar visita guiada"
"onboarding.help.keyboardShortcuts" = "Atajos de teclado"
"onboarding.tour.transport.title" = "Transporte"
"onboarding.tour.transport.body" = "Inicia y detén la reproducción, define el bucle y mira el cabezal."
"onboarding.tour.browser.title" = "Explorador"
"onboarding.tour.browser.body" = "Encuentra medios del proyecto y sonidos reutilizables."
"onboarding.tour.timeline.title" = "Línea de tiempo"
"onboarding.tour.timeline.body" = "Organiza clips y automatización en el tiempo."
"onboarding.tour.inspector.title" = "Panel inspector"
"onboarding.tour.inspector.body" = "Edita el clip, la pista y el ruteo seleccionados."
"onboarding.tour.mixer.title" = "Mezclador"
"onboarding.tour.mixer.body" = "Balancea pistas, envíos, medidores y salida maestra."
"onboarding.welcome.title" = "Bienvenida"
"onboarding.welcome.description" = "Elige una plantilla de proyecto o reabre un proyecto reciente."
"onboarding.welcome.projectTemplates" = "Plantillas de proyecto"
"onboarding.welcome.recentProjects" = "Proyectos recientes"
"onboarding.welcome.openProject" = "Abrir proyecto"
"onboarding.welcome.openRecent.help" = "Abre el proyecto de LaMusica más reciente."
"onboarding.chooseTemplateOrRecent" = "Elige una plantilla o abre un proyecto reciente."
"onboarding.welcome.openRecent" = "Abrir más reciente"
"onboarding.tour.skip" = "Omitir visita"
"onboarding.help.keyboardShortcuts.body" = "Espacio: Reproducir/Detener; Comando+O: Abrir; Comando+?: Ayuda"
"onboarding.help.userManual.fallback" = "El manual de usuario incluido se instala con LaMusica."
)";

void replaceAll(std::string& value, std::string_view from, std::string_view to) {
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
}

const std::string& frTable() {
    static const std::string table = [] {
        std::string value{enTable};
        replaceAll(value, "language: en\ncountries: US GB",
                   "language: fr\ncountries: FR\ncoverage: stub");
        replaceAll(value, "\"Main DAW window\" = \"Main DAW window\"",
                   "\"Main DAW window\" = \"Fenetre principale de LaMusica\"");
        replaceAll(value, "\"Play\" = \"Play\"", "\"Play\" = \"Lecture\"");
        replaceAll(value, "\"Start playback\" = \"Start playback\"",
                   "\"Start playback\" = \"Demarrer la lecture\"");
        replaceAll(value, "\"Stop\" = \"Stop\"", "\"Stop\" = \"Arret\"");
        replaceAll(value, "\"Stop playback\" = \"Stop playback\"",
                   "\"Stop playback\" = \"Arreter la lecture\"");
        replaceAll(value, "\"File\" = \"File\"", "\"File\" = \"Fichier\"");
        replaceAll(value, "\"Edit\" = \"Edit\"", "\"Edit\" = \"Edition\"");
        replaceAll(value, "\"View\" = \"View\"", "\"View\" = \"Affichage\"");
        replaceAll(value, "\"Help\" = \"Help\"", "\"Help\" = \"Aide\"");
        replaceAll(value, "\"Privacy\" = \"Privacy\"", "\"Privacy\" = \"Confidentialite\"");
        replaceAll(value, "\"status.ready\" = \"Ready\"", "\"status.ready\" = \"Pret\"");
        replaceAll(value, "\"privacy.shareDiagnostics\" = \"Share Diagnostics\"",
                   "\"privacy.shareDiagnostics\" = \"Partager les diagnostics\"");
        replaceAll(value, "\"privacy.keepPrivate\" = \"Keep Private\"",
                   "\"privacy.keepPrivate\" = \"Rester prive\"");
        replaceAll(value, "\"onboarding.template.empty.name\" = \"Empty\"",
                   "\"onboarding.template.empty.name\" = \"Vide\"");
        replaceAll(value, "\"onboarding.welcome.title\" = \"Welcome\"",
                   "\"onboarding.welcome.title\" = \"Bienvenue\"");
        replaceAll(value, "\"onboarding.welcome.openRecent\" = \"Open Most Recent\"",
                   "\"onboarding.welcome.openRecent\" = \"Ouvrir le plus recent\"");
        replaceAll(value, "\"onboarding.tour.skip\" = \"Skip Tour\"",
                   "\"onboarding.tour.skip\" = \"Ignorer la visite\"");
        return value;
    }();
    return table;
}

const std::array<BundledStringTable, 3>& tables() {
    static const std::array<BundledStringTable, 3> bundled{{
        BundledStringTable{.locale = "en", .resourceName = "en.txt", .contents = enTable},
        BundledStringTable{.locale = "es", .resourceName = "es.txt", .contents = esTable},
        BundledStringTable{.locale = "fr", .resourceName = "fr.txt", .contents = frTable()},
    }};
    return bundled;
}

} // namespace

std::string_view englishStringTable() noexcept {
    return enTable;
}

std::string_view spanishStringTable() noexcept {
    return esTable;
}

std::string_view frenchStringTable() noexcept {
    return frTable();
}

std::span<const BundledStringTable> bundledStringTables() noexcept {
    return tables();
}

} // namespace lamusica::daw::i18n
