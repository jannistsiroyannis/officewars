var controlPosition = "main menu"; // || side menu
function resize() {
    var canvas = document.getElementById("canv");
    canvas.width = window.innerWidth - 15;
    canvas.height = window.innerHeight - 20;

    if (controlPosition == "main menu") {
        var controlDiv = document.getElementById("control");
        var y = canvas.height / 2.0 - 200
        var x = canvas.width / 2.0 - 250
        controlDiv.style.top = y.toString() + "px";
        controlDiv.style.left = x.toString() + "px";
        controlDiv.style.height = "250px";
        controlDiv.style.width = "500px";
        controlDiv.style.backgroundColor = "rgba(255,255,255,1.0)";
        controlDiv.style.border = "1px solid #000000";
        controlDiv.style.textAlign = "left";
    } else // side menu
    {
        var controlDiv = document.getElementById("control");
        controlDiv.style.top = "15px";
        controlDiv.style.left = "15px";
        controlDiv.style.height = "250px";
        controlDiv.style.width = "500px";
        controlDiv.style.backgroundColor = "rgba(255,255,255,0.0)";
        controlDiv.style.border = "0px solid #000000";
        controlDiv.style.textAlign = "left";
    }
}
function start() {
    // Prevent right-click menu on canvas.
    var canvas = document.getElementById("canv");
    canvas.addEventListener('contextmenu', function (e) {
        if (e.button == 2) {
            e.preventDefault();
        }
    });

    window.onresize = function () {
        resize();
    };
    resize();
}