# Construcción en Windows x64

Requiere OBS Studio 32.2.2 instalado y Python 3. El filtro final no depende de Python. La prueba de GPU necesita el entorno gráfico local de Windows; no captura pantalla, cámara ni micrófono.

1. Descarga `https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip`. Comprueba su SHA-256 contra el campo `0.15.2 → x86_64-windows → shasum` de `https://ziglang.org/download/index.json`.
2. Descomprime en `build-tools/zig-x86_64-windows-0.15.2`.
3. Descarga `https://github.com/obsproject/obs-studio/archive/refs/tags/32.2.2.zip` y descomprime en `build-tools/obs-studio-32.2.2`.
4. Ejecuta `python build.py`. Si OBS está en otra ruta, establece `OBS_INSTALL_DIR` antes. Los archivos temporales y cachés se crean dentro de `build`.
5. Ejecuta `package.ps1` para crear el instalador y los ZIP. Usa el compilador .NET Framework de Windows. El instalador no está firmado.

Para las pruebas, desde PowerShell en la carpeta del proyecto:

```powershell
$env:PATH = 'C:\Program Files\obs-studio\bin\64bit;' + $env:PATH
New-Item -ItemType Directory -Force tests/artifacts | Out-Null
& ./build/gpu-test.exe 'C:\Program Files\obs-studio' "$PWD/dist/obs-sketch/bin/64bit/obs-sketch.dll"
```

La prueba abre una instancia de libobs aislada, sin cargar escenas ni perfiles del usuario. El resultado esperado es `RESULT: PASS (0 failures)`. `tests/preview.py` produce una comparación visual a partir de los buffers de GPU guardados; necesita Pillow y NumPy y no es necesario para compilar ni usar el filtro.

El instalador admite `/extract RUTA` para validar la extracción dentro de una carpeta de prueba, sin instalar en ProgramData. Usa los mismos recursos y comprobación SHA-256 que la instalación real.

## Traducciones (1.4)

`locale/strings.json` contiene las traducciones inglesa, española, italiana, francesa y alemana. `build.py` ejecuta `generate_translations.py`, valida que todas las entradas tengan cinco traducciones y genera `build/translations.h` para la DLL y `build/LocaleText.cs` para el instalador. Compila antes de ejecutar `package.ps1`.

Ambos componentes seleccionan el idioma mediante `GetUserDefaultUILanguage`, usando el idioma de visualización de Windows del usuario, independientemente del idioma de OBS o del formato regional. Las variantes regionales usan su idioma base y los idiomas no disponibles usan inglés. `build/locale-test.exe` comprueba 17 variantes y casos de reserva sin cambiar Windows. `tests/LocaleTest.cs`, compilado junto a `build/LocaleText.cs`, comprueba el instalador. La prueba GPU verifica las etiquetas del idioma real del sistema y que el último elemento del panel sea «A Plugin By DjEmiMas».

## Marca (1.5)

`assets/DjEmiMas-original.png` conserva el logo proporcionado por el autor. El PNG de 256 px y el ICO con tamaños de 16 a 256 px son conversiones de ese mismo archivo. `build.py` integra el PNG en la DLL como imagen de texto enriquecido de Qt; `package.ps1` integra ambos recursos en el instalador y establece el icono del ejecutable. No hay descargas ni dependencias de imágenes externas al usar el filtro.

## Prueba con las referencias locales

`python tests/reference-preview.py` recorta la mitad sin pintar de la foto local del baúl y la pasa por los cuatro acabados de la DLL real, con los mismos ajustes. Necesita Pillow y la foto `FOTOS/474212826_915991793852405_8164925281253054869_n.jpg` (no incluida en el paquete de código). Guarda `cuatro-acabados.png`, la comparación de recuperación del color y los registros en `tests/artifacts/reference`.

El ejecutable de pruebas admite `gpu-test OBS_INSTALL_ROOT PLUGIN_DLL INPUT_RGBA WIDTH HEIGHT OUTPUT_DIR` para comprobar una imagen RGBA adicional. El directorio de salida debe existir.

## Seguimiento de cámara (1.2)

`src/motion.h` implementa la selección de esquinas, búsqueda de parches, refinamiento subpíxel y ajuste robusto de una transformación de semejanza. Reutiliza fotogramas de referencia para reducir deriva por acumulación. El filtro crea una imagen de análisis de como máximo 192 píxeles de lado, conserva su proporción y la lee desde la GPU; el dibujo final usa siempre la fuente actual a resolución completa. Cada instancia tiene su propio historial y recursos.

`build.py` genera además `build/motion-test.exe`. Ejecútalo para validar desplazamiento, zoom, giro, estabilidad y cortes con transformaciones conocidas. `python tests/motion-preview.py` genera una secuencia con la foto local del baúl, compara seguimiento activado/desactivado en la DLL real y escribe una animación y el registro en `tests/artifacts/motion`. El modo de secuencia es `gpu-test OBS_INSTALL_ROOT PLUGIN_DLL INPUT_RGBA WIDTH HEIGHT OUTPUT_DIR --motion`.
