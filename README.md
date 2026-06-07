# Flappy Bird - Windows Native

Migrado de Linux/ncurses a Windows API nativa.

## Archivos

- `driver.c` - Código fuente migrado para Windows
- `driver_original.c` - Código fuente original (Linux/ncurses)
- `FlappyBird.exe` - Ejecutable pre-compilado

## Compilación

```bash
gcc driver.c -O2 -Wall -o FlappyBird.exe
```

Requiere MinGW GCC. Usar w64devkit o similar.

## Controles

- **Flecha Arriba** = Flap (volar)
- **Q** = Salir
- **Enter** = Reiniciar (al morir)

## Ejecución

Doble clic en `FlappyBird.exe` o ejecutar desde terminal.
