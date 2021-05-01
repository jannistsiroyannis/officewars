class ClientApi {
    constructor(rawApi) {
        this._rawApi = rawApi
    }

    async loadGame(path) {
        const data = await (await fetch(path)).text()
        const api = this._rawApi
        const game = api.loadGame(data, data.length)
        console.log(api.getGameName(game))
        return this._wrapGame(game)
    }

    async loadGameList(path) {
        const data = await (await fetch(path)).text()
        const api = this._rawApi
        const gameList = api.loadGameList(data, data.length)
        const listFuncs = {
            gameCount: api.getGameCount,
            game: this._wrapGame,
        }
        return Object.keys(listFuncs).reduce((acc, key) => {
            acc[key] = listFuncs[key].bind(acc, gameList)
            return acc
        }, {})
    }

    _wrapGame(game) {
        const api = this._rawApi
        const gameFuncs = {
            name: api.getGameName,
            id: api.getGameId,
            playerCount: api.getPlayerCount,
            playerName: api.getPlayerName,
            playerColor: api.getPlayerColor,
            nodeCount: api.getNodeCount,
            nodesConnected: api.getNodeConnected,
            nodeOwner: api.getNodeControlledBy,
            nodeX: api.getNodeX,
            nodeY: api.getNodeY,
            nodeZ: api.getNodeZ,
            turnCount: api.getTurnCount,
            stepHistory: api.stepGameHistory,
        }
        return Object.keys(gameFuncs).reduce((acc, key) => {
            acc[key] = gameFuncs[key].bind(acc, game)
            return acc
        }, {})
    }

    static async fromModule(module) {
        const apiInit = new Promise((resolve, reject) => {
            module.onRuntimeInitialized = () => {
                const loadGameList = Module.cwrap('loadGameList', 'number', ['string', 'number'])
                const loadGame = Module.cwrap('loadGame', 'number', ['string', 'number'])
                const getGameCount = Module.cwrap('getGameCount', 'number', ['number'])
                const getGame = Module.cwrap('getGame', 'number', ['number', 'number'])
                const getGameId = Module.cwrap('getGameId', 'string', ['number'])
                const getGameName = Module.cwrap('getGameName', 'string', ['number', 'number'])
                const getPlayerCount = Module.cwrap('getPlayerCount', 'number', ['number'])
                const getPlayerName = Module.cwrap('getPlayerName', 'string', ['number', 'number'])
                const getPlayerColor = Module.cwrap('getPlayerColor', 'string', ['number', 'number'])
                const getNodeCount = Module.cwrap('getNodeCount', 'number', ['number'])
                const getNodeConnected = Module.cwrap('getNodeConnected', 'number', ['number', 'number', 'number'])
                const getNodeControlledBy = Module.cwrap('getNodeControlledBy', 'number', ['number', 'number'])
                const getNodeX = Module.cwrap('getNodeX', 'number', ['number', 'number'])
                const getNodeY = Module.cwrap('getNodeY', 'number', ['number', 'number'])
                const getNodeZ = Module.cwrap('getNodeZ', 'number', ['number', 'number'])
                const getTurnCount = Module.cwrap('getTurnCount', 'number', ['number'])
                const stepGameHistory = Module.cwrap('stepGameHistory', 'number', ['number', 'number'])

                resolve({
                    loadGameList,
                    loadGame,
                    getGameCount,
                    getGame,
                    getGameId,
                    getGameName,
                    getPlayerCount,
                    getPlayerName,
                    getPlayerColor,
                    getNodeCount,
                    getNodeConnected,
                    getNodeControlledBy,
                    getTurnCount,
                    stepGameHistory,
                    getNodeX,
                    getNodeY,
                    getNodeZ,
                })
            }
        });
        const rawApi = await apiInit;
        return new ClientApi(rawApi)
    }
}