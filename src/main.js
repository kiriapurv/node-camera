var WebSocketServer = require("ws").Server;
var cam = require("../build/Release/camera.node");
var fs = require("fs");

var wss = new WebSocketServer({
    port: 9090
});

var clients = {};

var frameCallback = function (image) {
    var frame = {
        type: "frame",
        frame: new Buffer(image, "ascii").toString("base64")
    };
    var raw = JSON.stringify(frame);
    for (var index in clients) {
        clients[index].send(raw);
    }
};

var disconnectClient = function (index) {
    delete clients[index];
    if (Object.keys(clients).length == 0) {
        console.log("No Clients, Closing Camera");
        cam.Close();
    }
};

var connectClient = function (ws) {
    var index = "" + new Date().getTime();
    console.log(cam.IsOpen());
    if (!cam.IsOpen()) {
        console.log("New Clients, Opening Camera");
        cam.Open(frameCallback, {
            width: 640,
            height: 360,
            window: true,
            codec: ".jpg"
        });
    }
    clients[index] = ws;
    return index;
};

wss.on('connection', function (ws) {
    var disconnected = false;
    var index = connectClient(ws);

    ws.on('close', function () {
        disconnectClient(index);
    });

    ws.on('open', function () {
        console.log("Opened");
    });

    ws.on('message', function (message) {

        switch (message) {
        case "close":
            {
                disconnectClient(index);
            }
            break;
        case "size":
            {
                var size = cam.GetPreviewSize();

                ws.send(JSON.stringify({
                    type: "size",
                    width: size.width,
                    height: size.height
                }));
            }
            break;
        }
    });

});

//Create Http Server
var http = require("http");

http.createServer(function (req, resp) {
    resp.writeHead(200, {
        "Content-Type": "text/html"
    });
    resp.end(fs.readFileSync("../index.html"));
}).listen(9999);

console.log("Http Server Started");