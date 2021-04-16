var controlPosition = "main menu"; // || side menu
function resize() {
    const canvas = document.getElementById("canv");
    const controlDiv = document.getElementById("control");
    const contributeDiv = document.getElementById("contribute");
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;

    if (controlPosition == "main menu") {
        controlDiv.classList.add("menu")
        controlDiv.classList.remove("game")
        contributeDiv.classList.remove("hidden")
    } else {
        controlDiv.classList.remove("menu")
        controlDiv.classList.add("game")
        contributeDiv.classList.add("hidden")
    }
}
function start() {
    // Prevent right-click menu on canvas.
    const canvas = document.getElementById("canv");
    canvas.addEventListener('contextmenu', (e) => {
        if (e.button == 2) {
            e.preventDefault();
        }
    });

    window.onresize = function () {
        resize();
    };
    resize();
}