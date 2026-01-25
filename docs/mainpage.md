# OpenGothic Documentation {#mainpage}

This is the API documentation for OpenGothic, an open source reimplementation of Gothic I and II.

## Source Structure

- @ref MainWindow - Application entry point and render loop
- **game/** - Core game logic and session management
- **graphics/** - Rendering pipeline
- **world/** - World, NPCs, and game objects
- **ui/** - Menus and HUD

## Building the Docs

```
doxygen Doxyfile
```

Output is generated in `docs/html/`.
