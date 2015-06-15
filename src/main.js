var WebSocketServer = require("ws").Server;
var cam = require("../build/Release/camera.node");
var fs = require("fs");
var websocketPort = 9090,
    webPort = 9999,
    openBrowser = false,
    width = 640,
    inputString = "",
    height = 360;

//Gathering Arguments
process.argv.forEach(function (val, index, array) {
    switch (val) {
    case "-open":
        openBrowser = true;
        break;
    case "-wsport":
        websocketPort = parseInt(array[index + 1]);
        break;
    case "-webport":
        webPort = parseInt(array[index + 1]);
        break;
    case "-res":
        var res = array[index + 1].split("x");
        width = parseInt(res[0]);
        height = parseInt(res[1]);
        break;
    case "-input":
        inputString = array[index + 1];
        console.log(inputString);
    }
});

var wss = new WebSocketServer({
    port: websocketPort
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
            width: width,
            height: height,
            window: false,
            codec: ".jpg",
            input: inputString
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
var index = fs.readFileSync(__dirname + "/../index.html", 'utf8').replace("##webSocketPort", websocketPort);
http.createServer(function (req, resp) {
    resp.writeHead(200, {
        "Content-Type": "text/html"
    });
    resp.end(index);
}).listen(webPort);

console.log("Http Server Started");

if (openBrowser) {
    var spawn = require('child_process').spawn
    spawn('open', ['http://localhost:' + webPort]);
}