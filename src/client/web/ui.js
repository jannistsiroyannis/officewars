var controlPosition = "main menu"; // || side menu
const _games = {
    list: []
}
function resize() {
    canvas = document.getElementById("canv");
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
}

function getView(viewId) {
    const allViews = [
        "displaySecret",
        "registerFor",
        "gameMenu",
        "mainMenu",
    ];
    let view = null;
    allViews.forEach(vId => {
        const v = document.getElementById(vId);
        if (v && !v.classList.contains("hidden")) v.classList.add("hidden");
        if (vId === viewId) view = v;
    });
    if (view) view.classList.remove("hidden");
    return view;
}

function displaySecret({
    secret,
    gameId,
    action
}) {
    const view = getView("displaySecret");
    const confirm = document.getElementById("displaySecretConfirm");
    confirm.onclick = action;

    const secretText = document.getElementById("displaySecretSecret");
    secretText.innerHTML = secret;
}

function registerFor({
    register,
    cancel
}) {
    const view = getView("registerFor");
    const n = "0123456789ABCDEF";
    const colorPreset = "#" + Array.from({length: 6}).map(
        _ => n[Math.floor(Math.random() * 16)]
    ).join("");

    const color = document.getElementById("registerForColor");
    color.value = colorPreset;

    const name = document.getElementById("registerForName");
    name.value = "";
    
    const confirmButton = document.getElementById("registerForConfirm");
    confirmButton.onclick = () => {
        console.log(
            color.value,
            name.value,
        )
        register(
            color.value,
            name.value,
        );
    };
    const cancelButton = document.getElementById("registerForCancel");
    cancelButton.onclick = cancel;
}

function gameMenu() {
    const view = getView("gameMenu");
}

function addGame(game) {
    const view = getView("mainMenu");
    _games.list.push(game);
    console.log(_games.list);

    view.innerHTML = _games.list.map(game => (
        {
            0: (
                `<div class="game-entry"> \
                    <b>Open to join: </b>${game.name}<br/><b>${game.count}</b> players have joined<br/>
                    ${!game.inGame ? (
                        `<button type="button" id="join_${game.id}">Join game</button>`
                    ) : (
                        `You have already joined this game.`
                    )}
                </div>`
            ),
            1: (
                `<div class="game-entry">
                    <b>In-game: </b>${game.name}<br/><b>${game.count}</b> players<br/>
                    <button type="button" id="view_${game.id}">
                        Enter/observ
                    </button>
                </div>`
            )
        }[game.state]
    )).join("");
    _games.list.forEach(game => {
        if (!game.inGame && game.state === 0) {
            const gameButton = document.getElementById(`join_${game.id}`);
            gameButton.onclick = game.join;
        } else if (game.state === 1) {
            const gameButton = document.getElementById(`view_${game.id}`);
            gameButton.onclick = game.view;
        }
    })
}

function mainMenu() {
    const view = getView("mainMenu");
    _games.list = []
}

function start() {
    // Prevent right-click menu on canvas.
    const canvas = document.getElementById("canv");
    

    window.onresize = function () {
        resize();
    };
    resize();
}