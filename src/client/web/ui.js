var controlPosition = "main menu"; // || side menu
var themes = [
    "base",
    "dark"
]
function resize() {
    const canvas = document.getElementById("canv");
    const controlDiv = document.getElementById("control");
    const contributeDiv = document.getElementById("contribute");
    const themeDiv = document.getElementById("theme");
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;

    if (controlPosition == "main menu") {
        controlDiv.classList.add("menu")
        controlDiv.classList.remove("game")
        contributeDiv.classList.remove("hidden")
        themeDiv.classList.remove("hidden")
    } else {
        controlDiv.classList.remove("menu")
        controlDiv.classList.add("game")
        contributeDiv.classList.add("hidden")
        themeDiv.classList.add("hidden")
    }
}

function setTheme(themeId) {
    const theme = document.getElementById("theme");
    theme.innerHTML = `
<select id="theme-select">
    ${themes.map(t => `<option value="${t}" ${t === themeId ? "selected" : ""}>${t}</option>`).join("\n")}
</select>
    `
    const themeSelect = document.getElementById("theme-select")
    themeSelect.onchange = handleSetTheme
    const body = document.getElementById("main")
    for (const t of themes) {
        body.classList.remove("theme-" + t)
    }
    body.classList.add("theme-" + themeId)
}

function handleSetTheme(e) {
    const themeId = e.target.value
    const urlParams = new URLSearchParams(window.location.search)
    urlParams.set("theme", themeId)
    history.pushState(null, null, "?" + urlParams.toString())
    setTheme(themeId)
}

function start() {
    // Prevent right-click menu on canvas.
    const canvas = document.getElementById("canv");
    canvas.addEventListener('contextmenu', (e) => {
        if (e.button == 2) {
            e.preventDefault();
        }
    });
    const urlParams = new URLSearchParams(window.location.search)
    const themeId = urlParams.get("theme")
    setTheme(themeId || themes[0])

    window.onresize = function () {
        resize();
    };
    resize();
}
