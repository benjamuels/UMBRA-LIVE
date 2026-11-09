# Destinos de matrix: auditoría y diseño contextual

Base revisada: ADD V10.6c-mod-eafc5e6-preview, fuente
`eafc5e671423b6ca7c0a9667fce5e9bd0934ec41`. Esta revisión añade un diagnóstico
host y el diseño; no cambia el firmware ni los instaladores.

## Comprobación realizada

Se recorrieron los 31 destinos no-OFF de los 12 motores disponibles: 372
combinaciones, cada una con AMT -64 y +63 (744 casos), comparadas con una ruta
OFF. Cada caso arranca en un proceso nuevo para compartir el mismo estado
inicial de osciladores y azar. Se renderizan 128 bloques con el primer preset,
una nota y señal USB sintética cuando corresponde; se comparan hashes exactos
de las muestras. El mix se deja seco para aislar el motor.

Resultado bruto: `build/modulation-audit/destinations.csv`.
Diagnóstico reproducible: `tests/audit_matrix_destinations.c`.
No es un certificado de que cada destino funcione en todos los modos: una salida
idéntica puede deberse a un parámetro condicionado, un límite o un bus vacío.
El dictamen requiere leer el consumidor de cada valor.

## Fallas y ambigüedades confirmadas por lectura y audio

- SHP no es leído por DRUM-X, ANIMAL ni INPUT. Estas rutas no hacen lo que promete
  un destino de timbre/shape. En DRUM-X existe un ruteo LFO SHAPE propio, pero
  `drum_render` calcula su contribución directamente: no lee `m->shape` de matrix.
- CUT de ADD no es consumido: ADD tiene un filtro de pista mediante F.CUT, pero
  no lee `m->cutoff`. El hash permanece idéntico con ambas profundidades.
- PITCH de matrix no llega a la afinación de DRUM-X: el motor usa TUNE, afinación
  de lane y su ruteo LFO/punch propio. El hash permanece idéntico.
- FM6 SLOT cambia temporalmente P_E7 pero no carga el patch: `fm6_sync` consume
  E0–E6 y la carga del banco es otra operación. No debe ofrecerse como si fuera
  una modulación continua de patches.
- CUT se usa como cutoff en ANALOG/LOFI/NOISE; como DCW en PHASE, profundidad FM
  en FM6 y desplazamiento de vocales en VOICE. ANIMAL lo usa sólo en ciertos
  modos. Los cuatro motores remapeados a F.CUT en la beta tienen duplicación.
- SHP tampoco es un destino uniforme: BUZZ en VOICE, COLOR en NOISE, DRIVE en
  WHEEL, COMB en ADD, BRIGHT/SIZE en STRING según modo, y una combinación de
  feedback y profundidad FM en FM6. Un nombre como TIMBRE no explica la ruta.
- INPUT THRU no pasa por voces: AMP, PITCH y VIB no procesan ese audio continuo.
  LEVEL sí llega al mixer. RING depende del render con teclas; WIDTH depende de
  entrada estéreo y su ruta. No retirar estas opciones universalmente de INPUT.

## Revisión de los 12 motores: CUT/SHP y parámetros propios

| Motor | CUT actual | SHP actual | Tratamiento contextual |
|---|---|---|---|
| ANALOG | Cutoff por voz | Morph/forma según onda | Distinguir el offset de voz del parámetro CUT; reutilizar WAVE, MIX, RES, DRIVE e INT |
| PHASE | DCW | Otra contribución a DCW | Mostrar destino DCW principal; conservar rutas antiguas; LINE es discreto |
| LOFI | Filtro por voz | Morph o variación según chip/onda | Reutilizar `lofi_desc`: ALGO/VAR/MOTN/GRIT/DCY, ocultar el slot marcado `-` |
| VOICE | Desplaza vocales en modo vocal | BUZZ | Separar modo vocal y WORDS; VOWEL2 requiere TALK; WORDS usa POS/THEME/ARTIC/LOOP |
| WHEEL | Alias del filtro de pista | DRIVE | Un filtro visible; DRIVE explícito; REG, PERC y ROTOR son discretos |
| STRING | Alias del filtro de pista | BRIGHT; también SIZE en feedback | Reutilizar `string_desc`: MOVE, GAIN, DIST, SPEAKR y BLOOM en feedback; POS inicial puede no responder durante la nota |
| DRUM-X | Alias del filtro de pista | Sin consumidor | Ofrecer MORPH A/B, TUNE, TONE, DECAY, NOISE, FM ENV, FOLD y DRIVE; retirar PITCH/SHP de nuevas selecciones |
| NOISE | Frecuencia interna | COLOR | Nombres FREQ y COLOR; `noise_desc` adapta los controles de LFSR/META |
| FM6 | Profundidad FM | Feedback + profundidad FM | Priorizar MOD y FB explícitos; SLOT no válido como destino de matrix; RATIO/VMOD dependen del patch |
| ADD | Sin consumidor | COMB | CUT de pista mediante F.CUT; parámetros reales MODE, MORPH, BRIGHT, PARTIAL, SHIFT, COMB, DECAY y MOTION |
| ANIMAL | Sólo ciertos modelos | Sin consumidor | No ofrecer SHP; CUT interno sólo en los modos que lo usan; controles propios dependen del modelo |
| INPUT | Alias del filtro de pista | Sin consumidor | En THRU usar GAIN/GATE/BITS/RATE/F.CUT/LEVEL y los controles de ruta aplicables; AMP/PITCH/RING según KEYS |

MORPH no es una función genérica compartida: DRUM-X interpola los parámetros de
los patches A y B de cada lane (`drumx_voice.c:dx_run`), mientras ADD interpola
las relaciones de parciales entre MODE y el siguiente (`eng_add.c:add_render`).
Se puede conservar MORPH en la página del motor, pero en matrix debe compartir
ese mismo contexto, sin introducir otro destino genérico llamado TIMBRE.

## Diseño propuesto

1. **Una lista de selección por motor y modo.** Usar `track_desc` para los nombres,
   rangos y controles que ya existen. Esa función ya adapta STRING, LOFI, NOISE
   y VOICE. Saltar descriptores vacíos, `-`, rango cero y rutas sin consumidor.
   Un destino debe referirse a un control real o indicar expresamente su alcance.
2. **Evitar duplicaciones en nuevas selecciones.** Si CUT/F.CUT/E5 identifican el
   mismo control o SHP duplica un control del motor, ofrecer la ruta explícita
   principal. No convertir silenciosamente un offset por voz en un cambio del
   parámetro base: con ENV/VEL son operaciones distintas. Las rutas por voz que
   se conserven deben indicarlo y tener el nombre del parámetro que afectan.
3. **Condiciones, no falsas promesas.** Distinguir continuo, discreto y al disparar
   nota; activar la disponibilidad según el modo base. Las dependencias como
   TALK, patch FM6 o señal de entrada no se pueden validar sólo con un hash.
   Los destinos de sends siguen siendo válidos si su bus está apagado, pero
   deben nombrarse como envíos. No esconderlos por no producir audio ahora.
   RATE genérico es la velocidad del LFO (propuesta: L.RATE), y VIB es la
   profundidad de pitch de ese LFO (L.PIT); no confundirlos con RATE/VIB propios
   de INPUT, ANIMAL o LOFI. AMP actúa por voz; LEVEL sobre la pista completa.
4. **Master claramente separado.** CLIP/PNCH y D.TIM/D.FB/R.SIZ/etc afectan al
   master/buses compartidos, no sólo a la pista donde se coloca el slot. Deben
   tener una indicación compacta de ese alcance.
5. **Compatibilidad.** Conservar IDs y bytes guardados. Filtrar la navegación,
   no renumerar el enum. Las rutas antiguas no disponibles se muestran como
   tales y se conservan hasta editarlas; no reasignarlas a otro control al cargar
   un proyecto o cambiar engine. Mantener respuesta por voz de rutas válidas.
6. **Un criterio compartido.** El selector, la asignación directa desde un control,
   la generación de macros/DICE y los metadatos del editor deben usar la misma
   validez. Hoy `macro_dice` filtra algunos E vacíos, pero aún puede elegir SHP
   donde no tiene consumidor. El editor no debe mostrar un destino distinto
   del firmware para un mismo ID.

## Memoria y CPU

El nombre y rango del parámetro ya están almacenados: reutilizarlos evita una
segunda tabla grande de textos. Una máscara de los 32 IDs por motor ocuparía
64 bytes de flash para los 16 IDs de engine (incluidos los reservados); reglas
por modo y el código de navegación añaden más. Una representación por excepciones
puede ser menor. Es una estimación de diseño, no el tamaño final compilado.

El filtrado se ejecutaría al elegir un destino o cambiar modo, fuera del bucle
por muestra. No necesita buffers de audio, voces nuevas ni una lista persistente
por pista; no existe una razón para añadir carga DSP por esta interfaz.

La imagen publicada ocupa 584304/584336 bytes: quedan sólo **32 bytes de flash**.
Aunque los metadatos sean pequeños, el nuevo código no cabe automáticamente.
Hay que medir el cambio completo, recuperar espacio y pasar el link normal,
layout y checks de RAM antes de publicarlo. La reserva del pool no es espacio
para gastar ni se deben bajar los márgenes exigidos.

## Pruebas que exige la implementación

- Para cada modo: todos los destinos ofrecidos tienen consumidor real y sus
  etiquetas/rangos coinciden con la página del motor; ninguna etiqueta `-`/E
  genérica tapa un parámetro ausente.
- Mantener IDs y round-trip de proyectos/presets/editor con rutas antiguas.
- Cambio de motor/modo sin sobrescribir rutas, parámetros ni notas sostenidas.
- Selección bidireccional, saltos de perilla, límites y macros aleatorias.
- Audio para controles continuos, discretos y de ataque, con condiciones activas:
  TALK, INPUT THRU/KEYS, modos de ANIMAL/LOFI/STRING y patches FM6 apropiados.
- No exigir cambio audible a un send sin retorno ni confundir un patch sin
  sensibilidad a velocidad con un destino VMOD universalmente inválido.
- Preservar las fuentes T1 ENV–T6 ENV en todas las rutas válidas.

## Implementación de prueba

La implementación mantiene los 32 IDs originales. `mod_dst_offer` filtra sólo las
selecciones nuevas, la asignación desde FOCUS y DICE; no se usa para borrar o
remapear rutas durante el audio ni la carga de proyectos. `mod_dst_name` conserva
nombres específicos para las rutas antiguas válidas y muestra N/A si el modo no
tiene consumidor. Cargar un preset sigue el comportamiento anterior de carga de
sonido; esta política no cambia qué parámetros reemplaza un preset.

La página de matrix indica VOICE/TRACK/MASTER; su diagrama mantiene D1–D4
como números de ruta para no confundirlos con pistas. Los
nombres de E1–E8 provienen de `track_desc`; CUT/SHP reciben los nombres de sus
consumidores concretos. Se conservan controles por voz que tienen un uso distinto
del parámetro base por pista. Los aliases exactos se omiten en nuevas selecciones;
las antiguas rutas compuestas FM6 SHP y STRING feedback SHP mantienen sus efectos
con los nombres FB+MOD y BR+SIZE, pero ya no se ofrecen como opciones nuevas.
L.RATE y L.PIT distinguen los controles del LFO de los parámetros del motor.

SLOT/RATIO/VMOD de FM6, POS de STRING fuera de feedback, controles binarios y los
controles propios de INPUT que no procesan el modo actual se omiten de nuevas
rutas. Se mantienen dependencias musicales legítimas (por ejemplo VOWEL2/TALK,
fuentes de entrada o sends con sus retornos apagados). El modo base determina
la lista; una ruta que module MODE puede modificar temporalmente las condiciones
bajo las que actúa otra ruta.

Los rangos de destinos propios ahora consultan el descriptor del modo en el
render. Ejemplo corregido: ANALOG de tres osciladores usa INT 3 de -24 a +24,
no los límites 0..127 de NOISE que ocupaba ese slot en la onda básica. El valor
base se restaura después del bloque como antes.

ED_DESC devuelve los nombres por contexto con prefijos V:/T:/M:. Los IDs ocultos
se anuncian como aliases de OFF; si uno está guardado en ese slot, se mantiene
visible su nombre o N/A. ED_SET conserva los IDs crudos para importar archivos
antiguos. El editor beta actualiza también los cuatro descriptores de matrix al
cambiar un modo, LANG o destino, incluyendo cambios recibidos del aparato. No
cambia el protocolo ni la disposición de los proyectos.

La tabla ANIMAL de overrides se representa con 15 descriptores y un mapa pequeño,
en lugar de 40 descriptores con 25 vacíos. Los nombres y rangos son los mismos.
La conexión de matrix sigue animándose con una línea y un pulso más simples.
Las reglas UI se alojan en el banco secundario existente mediante BTAPP_FN; las
entradas comprueban `bt_hw_seg` (identidad/CRC cacheada) antes de llamarlo. Si el
banco está ausente o no coincide, se vuelve al selector genérico anterior sin
llamar código de otro paquete. Esto no inicia Bluetooth ni agrega buffers,
voces o estado persistente a las pistas.

Pruebas nuevas: 774 configuraciones de modos, selección en ambos sentidos y con
saltos grandes, destinos concretos y aliases, FOCUS/DICE, persistencia de IDs,
rango firmado de INT 3, protocolo SysEx con nombres por contexto, restauración
por SET de rutas antiguas, sintaxis JS y actualización de descriptores del editor.
Además se ejecutan los checks existentes de matrix/followers, almacenamiento,
canción, USB, MIDI, split, timing y comparación con la deuda conocida de UI.
El tamaño objetivo y la publicación se registran en RESULT.md cuando terminen.
