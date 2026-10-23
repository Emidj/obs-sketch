# OBS Sketch 1.5

Filtro nativo instalable para OBS Studio en Windows x64, con cuatro acabados, seguimiento del movimiento de cámara y recuperación progresiva del color original. En OBS se llama **OBS Sketch**.

## Instalación

1. Cierra OBS.
2. Abre `OBS-Sketch-1.5-Windows-x64-Setup.exe` y pulsa **Instalar**.
3. Abre OBS. En la fuente que quieras transformar: **Filtros → Filtros de efecto → + → OBS Sketch**.
4. Elige directamente una opción en **Acabado**. Los filtros nuevos empiezan en «Pintura blanca y rotulador».

No necesitas Lua, Shaderfilter, Python ni conexión a Internet para usar el filtro. El instalador incluye el shader dentro de la DLL. Copia únicamente los archivos de este filtro a `C:\ProgramData\obs-studio\plugins\obs-sketch` y permite desinstalarlo desde el mismo ejecutable. No modifica escenas ni perfiles.

La compilación está probada con **OBS 32.2.2, Windows x64 y Direct3D 11 en una NVIDIA GeForce RTX 5070**. El instalador es local y no tiene firma digital. Otras versiones de OBS y otras tarjetas gráficas requieren validación adicional.

Para OBS portable, copia `obs-sketch.dll` desde el ZIP a `obs-plugins\64bit` dentro de la instalación portable. El shader está integrado, por lo que no necesita archivos de datos para funcionar.

## Idioma y autor

Los deslizadores, acabados, descripciones, opciones de seguimiento y mensajes del filtro se muestran automáticamente en **español, inglés, italiano, francés o alemán**, según el idioma de visualización de Windows del usuario. Las variantes regionales comparten traducción: por ejemplo, francés de Francia o Canadá. Otros idiomas utilizan inglés. Se usa el idioma de Windows, aunque OBS esté configurado en otro idioma; no se usa el formato regional de fechas o números. Reinicia OBS después de cambiar el idioma de Windows.

El instalador también está traducido a los cinco idiomas. Las traducciones están integradas en la DLL y en el instalador, sin archivos de idioma externos. Este manual se mantiene en español. El nombre **OBS Sketch** y el crédito **A Plugin By DjEmiMas**, situado al final del panel del filtro, se conservan en todos los idiomas.

La versión 1.5 incluye el logo de DjEmiMas junto al crédito tanto en el instalador como al pie del filtro. El ejecutable del instalador utiliza ese mismo logo como icono de Windows. Las imágenes están integradas y no necesitan archivos externos.

## Transparencia y orden de filtros

Colócalo **después del croma, eliminación de fondo o máscara alfa**, para que reciba la transparencia ya calculada. Mantiene el alfa del píxel central, también en bordes suaves y elementos semitransparentes. El contorno de la silueta se pinta hacia dentro; no expande la máscara. «Pintura blanca» cambia el color visible, sin añadir un fondo opaco.

El filtro trabaja con el RGB desmultiplicado, ignora el color oculto de vecinos totalmente transparentes y vuelve a premultiplicar al componer. El formato SDR habitual conserva el alfa a precisión de 8 bits. Fuentes SDR de alta precisión usan una textura de 16 bits flotantes. HDR se deja pasar sin modificar.

Cada instancia tiene ajustes independientes. Puedes aplicarlo a una cámara, imagen, vídeo, captura, grupo o escena. Si lo aplicas a una escena compuesta, transforma la escena completa; para conservar un recorte, aplícalo a la fuente recortada.

## Acabados

- **Cómic a color:** simplifica la iluminación en bandas, conserva los tonos del material y añade contornos y rayado. «Niveles de sombreado» controla la simplificación y «Saturación» la intensidad del color.
- **Boceto blanco y negro:** convierte la imagen a grises y añade sombras, contornos y rayado. Conserva las zonas oscuras del objeto; no las repinta de blanco.
- **Tinta sobre color original:** conserva el color y el detalle fotográfico originales y superpone tinta y rayado. No posteriza, aclara ni cambia la saturación de la base. Tinta cero devuelve el original.
- **Pintura blanca y rotulador:** repinta las superficies de blanco, limita las sombras grises y añade contornos y grupos de rayas. Este acabado se desarrolló observando las fotos de `FOTOS`, como aproximación automática al estilo de objetos pintados por CarSplashART; no es una reproducción exacta ni un producto afiliado al creador.

La pintura blanca solo actúa en su acabado. La versión 1.3 corrige que un valor guardado de pintura blanca igual a 1 borrase las diferencias entre los otros tres acabados. Al cambiar «Acabado», «Revelar color original» vuelve a cero para mostrar el resultado propio del nuevo acabado; los demás ajustes se conservan y solo se muestran los controles pertinentes. Abrir el filtro o recargar la escena conserva el color revelado si no se cambia de acabado. La actualización conserva el identificador interno para que las escenas sigan reconociendo el filtro.

Los valores iniciales incluyen grosor **2,4**, umbral **0,30**, tinta **1**, tres niveles de sombras, suavizado **0,90**, pintura blanca **1**, rayado **0,85**, separación **11 px**, irregularidad **0,40**, acentos blancos **0,35** y contorno de silueta **1**. Los controles propios de «Pintura blanca y rotulador» son:

- **Gris de las sombras** (0,20): conserva volumen sobre la pintura blanca; cero deja las superficies blancas.
- **Trazos finos en hendiduras** (0,40): añade detalle interior a los contornos gruesos.
- **Longitud de grupos de rayas** (48 px) y **Cantidad de grupos de rayas** (0,35): controlan la longitud y la ocupación del rayado. Cantidad cero elimina los grupos.
- **Negro en huecos profundos** (0,85): oscurece zonas muy oscuras de la entrada. No identifica semánticamente un hueco.

Si aparecen demasiadas vetas o detalles de cámara, sube el umbral. Si faltan juntas o bordes, bájalo. Si hay demasiadas rayas, baja **Cantidad de grupos de rayas**; si son demasiado oscuras, baja **Rayado de sombras**. **Saturación** solo se muestra en «Cómic a color».

Los controles se miden en píxeles de la fuente. Ajusta grosor, separación y longitud si cambia su resolución. Intensidad cero omite el procesamiento del filtro.

## Comparación real

`tests/artifacts/reference/cuatro-acabados.png` compara los cuatro acabados sobre la misma foto del baúl **sin pintar**, procesados por la DLL real en Direct3D 11. Se utilizan los mismos ajustes en todos, cambiando únicamente «Acabado», con pintura blanca guardada a 1 y «Revelar color original» a 0. No se utiliza la parte ya pintada como entrada ni una imagen generada para simular el resultado.

## Movimiento de cámara y color original (1.2)

**Adaptar rayas al movimiento de cámara** está activado de forma predeterminada, también al actualizar desde 1.1. Estima el desplazamiento, giro y zoom general entre fotogramas y aplica ese movimiento al rayado de los cuatro acabados y a la irregularidad de la tinta. Las sombras se recalculan a partir de la imagen actual; el acabado de pintura blanca usa transiciones suaves entre sus niveles para reducir saltos al moverse. No mezcla imágenes antiguas sobre la actual ni altera el alfa.

**Reanclar rayas a la imagen actual** reinicia la posición del patrón si el seguimiento se pierde. Un cambio de resolución, una pausa de más de medio segundo sin renderizar, un corte detectado, o desactivar y activar el seguimiento también reinician el anclaje. En imágenes sin suficientes detalles, se mantiene la última transformación conocida hasta poder recuperar el seguimiento. Desactivar la casilla recupera la trama fija.

El seguimiento es global, no una reconstrucción 3D ni seguimiento independiente de cada objeto. Cambios fuertes de perspectiva, movimientos muy rápidos, superficies lisas o varios objetos moviéndose por separado pueden producir deslizamiento. El análisis utiliza una imagen reducida y añade coste de procesamiento; el resultado debe comprobarse con la cámara y resolución que se vayan a usar.

**Revelar color original (conservar tinta)** recupera progresivamente el color bajo el dibujo: **0** conserva el acabado elegido y **1** devuelve el color original con los contornos y rayas encima. Se muestra en los acabados de cómic, boceto y pintura blanca. «Tinta sobre color original» ya usa el color original y oculta este control. Al seleccionar otro acabado, el deslizador vuelve a cero automáticamente. Para recuperar la imagen completamente sin dibujo, usa **Intensidad del efecto = 0**.

`tests/artifacts/motion/seguimiento-camara.gif` muestra la DLL con un desplazamiento de cámara simulado sobre el baúl, con y sin seguimiento. En esa prueba, el error medio del patrón alineado bajó de 7,20 a 0,38 niveles de gris (aproximadamente un 95 %). Esta cifra describe esa prueba, no una garantía para cualquier vídeo. `tests/artifacts/reference/revelar-color.png` muestra el control de color a cero y al máximo.

## Alcance del efecto

Es una aproximación procedural en tiempo real. No usa IA generativa, no entiende la geometría de los objetos ni reproduce las decisiones artísticas de un pintor. No puede inventar fielmente las líneas de paneles, arañazos ni la perspectiva de las rayas dibujadas a mano. Aplica el acabado a toda la fuente visible: para tratar solo un objeto, necesita recibir una máscara o recorte previo. El acabado nuevo realiza más muestreos que los antiguos; no se ha medido un rendimiento garantizado a 1080p/4K o 60 fps.

Referencias observadas:
- https://www.facebook.com/reel/658836213438931/ — Fiat 500, pintura blanca/gris y rotulador negro.
- https://www.facebook.com/reel/3724407061025197/ — recopilación de pinturas del creador.
- https://docs.obsproject.com/reference-sources — API de filtros de OBS.

## Código y comprobaciones

`src/obs-sketch.c` implementa el filtro nativo; `plugin/obs-sketch.effect` contiene el shader. `build.py` compila con Zig 0.15.2 y las cabeceras oficiales de OBS 32.2.2. La DLL se enlaza a `obs.dll`, incluida con OBS. `BUILD.md` explica cómo reconstruirlo.

`tests/gpu-test.c` carga la DLL real con libobs y Direct3D 11 y comprueba los cuatro modos en dos tamaños, los 256 niveles de alfa, ausencia de color en píxeles transparentes e identidad al poner intensidad cero. Compara los seis pares de acabados usando los mismos ajustes y comprueba color en los modos de color, grises neutros en el boceto, identidad de la base fotográfica sin tinta, visibilidad de controles y el nombre «OBS Sketch». También verifica la ausencia del botón retirado, pintura neutra, aclarado de un material oscuro, estabilidad de imágenes estáticas, rayado e intensidad intermedia. Los resultados se guardan en `tests/artifacts`.

La versión 1.2 añade comprobaciones de recuperación del color con y sin tinta y una secuencia de desplazamiento con la DLL real. `tests/motion-test.c` verifica el estimador con desplazamientos, giros, zoom, imágenes repetidas y cortes conocidos.

Licencia: GPL-2.0-o-posterior. Se incluye el código fuente correspondiente y una copia de la licencia.
