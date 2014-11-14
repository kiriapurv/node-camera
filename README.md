## Node Camera

Access and stream web cam in nodejs using opencv and web sockets.

#### Building

- It required opencv headers and library to build and run.
- Modify `include_dirs` for headers path and `library_dirs` for library path in `binding.gyp` according to your opencv installation.
- Then install modules with `npm install`

#### Running

```
npm start -- [-open] [-wsport websocketPort] [-webport webserverport] [-ip ipaddress/domain] [-res widthxheight]
```

| Option | Description |
|---|---|
|-open | Open streaming url on startup |
|-wsport | Web socket port for streaming media |
|-webport | Web server port |
|-ip | Domain name/ip for non-localhost clients |
|-res | Resolution for preview image |