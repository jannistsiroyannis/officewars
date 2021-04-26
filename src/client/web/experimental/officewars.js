class Player {
    constructor({
        name,
        color,
        secret
    }) {
        this.name = name
        this.color = color
        this.secret = secret
    }

    static parse(data) {
        const [
            playerCount,
            ...rest
        ] = data.split("\n")

        let currentLines = rest
        return Array.from({
            length: parseInt(playerCount)
        }).map(() => {
            const [
                name,
                color,
                secret,
                ...lastRest
            ] = currentLines
            currentLines = lastRest
            return new Player({
                name,
                color,
                secret
            })
        })
    }

    static requiredLines(data) {
        const count = parseInt(data.match(/\d+/)[0])
        return count * 3 + 1
    }
}

class Node {
    constructor({
        id,
        connections,
        position
    }) {
        this.id = id
        this.connections = connections
        this.position = position
    }

    static parse(data) {
        const [
            countString,
            ...dataList
        ] = data.split("\n")
        const count = parseInt(countString)
        const couplingMatrix = dataList.slice(0, count)
        const idCouplings = couplingMatrix.flatMap((data, index) => (
            Node.parseCoupling(index, data)
        ))
        const positionList = dataList.slice(count, 2 * count)

        const positions = positionList.map(Node.parsePosition)
        const nodes = positions.map((p, index) => new Node({
            id: index,
            position: p,
            connections: []
        }))

        const couplings = idCouplings.map(idc => (
            idc.map(id => nodes[id])
        ))
        for (const node of nodes) {
            const connections = couplings.filter(
                c => c.includes(node)
            ).map(c => c.filter(n => n != node)[0])
            node.connections = connections
        }

        return nodes
    }

    static parsePosition(data) {
        return data.split(",").map((v, i) => parseFloat(v) * 10 * (i === 2 ? -1 : 1))
    }

    static parseCoupling(pos, data) {
        const nodeIds = data.split("").map((v, index) => (
            v === "1" ? index : -1
        ))
        return nodeIds.slice(pos).filter(
            id => id >= 0
        ).map(
            id => [pos, id]
        )
    }

    static requiredLines(data) {
        const count = parseInt(data.match(/\d+/)[0])
        return count * 2 + 1
    }
}

class Game {
    constructor({
        id,
        name,
        state,
        players,
        nodes
    }) {
        this.id = id
        this.name = name
        this.state = state
        this.players = players
        this.nodes = nodes
    }


    static parse(data) {
        const [
            id,
            name,
            state,
            _,
            ...gameContent
        ] = data.split("\n")

        const playerLines = Player.requiredLines(gameContent[0])
        const playerData = gameContent.slice(0, playerLines).join("\n")
        const players = Player.parse(playerData)

        const nodeLines = Node.requiredLines(
            gameContent[playerLines]
        )
        const nodeData = gameContent.slice(
            playerLines,
            playerLines + nodeLines
        ).join("\n")
        const nodes = Node.parse(nodeData)

        return new Game({
            id,
            name,
            state,
            players,
            nodes,
        })
    }

    static async loadGame(url) {
        const data = await (await fetch(url)).text()
        const game = Game.parse(data)
        console.log(game)
        return game
    }
}
