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
        ] = data

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

    static fromGameInspector(gi) {
        const playerCount = gi.playerCount()
        return Array.from(
            {length: playerCount}
        ).map((_, index) => new Player({
            name: gi.playerName(index),
            color: gi.playerColor(index),
            secret: ""
        }))
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
        ] = data
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
        const count = parseInt(data[0])
        return count * 2 + 1
    }

    static fromGameInspector(gi) {
        const nodeCount = gi.nodeCount()
        const nodes = Array.from(
            {length: nodeCount}
        ).map((_, index) => new Node({
            id: index,
            position: [
                gi.nodeX(index),
                gi.nodeY(index),
                gi.nodeZ(index)
            ].map((v, i) => v * 10 * (i === 2 ? -1 : 1)),
            connections: []
        }))

        for (const node of nodes) {
            for (const coupleNode of nodes) {
                const isCoupled = gi.nodesConnected(node.id, coupleNode.id)
                if (isCoupled === 1) {
                    node.connections.push(coupleNode)
                }
            }
        }

        return nodes;
    }
}

class Action {
    constructor({
        from,
        to,
        player,
        type
    }) {
        this.from = from
        this.to = to
        this.player = player
        this.type = type
    }

    static parse(data, players, nodes) {
        const roundCount = parseInt(data[0])
        let position = 1
        const rounds = []
        for (let r = 0; r < roundCount; r++) {
            const actionCount = parseInt(data[position])
            position += 1
            const actions = []
            for (let i = 0; i < actionCount; i++) {
                const [
                    playerId,
                    fromId,
                    toId,
                    typeId
                ] = Array.from({length: 4}).map(
                    (_, index) => parseInt(data[position + i * 4 + index])
                )
                const player = players[playerId]
                const from = nodes[fromId]
                const to = nodes[toId]
                const type = ["attack", "support"][typeId]
                actions.push(new Action({
                    player,
                    from,
                    to,
                    type
                }))
            }
            rounds.push(actions)
            position += actionCount * 4
        }
        return rounds
    }

    static requiredLines(data) {
        const rounds = parseInt(data[0])
        let position = 1
        for (let r = 0; r < rounds; r++) {
            const actionCount = parseInt(data[position])
            position += actionCount * 4 + 1
        }
        return position
    }

    static fromGameInspector(gi, players, nodes) {
        return []
    }
}

class State {
    constructor({
        ownedBy
    }) {
        this.ownedBy = ownedBy
    }

    static parse(data, players, nodes) {
        const count = parseInt(data[0])
        return Array.from({length: count}).map((_, i) => {
            const ownerIds = data[1 + i].split(",").map(v => parseInt(v))
            return new State({
                ownedBy: ownerIds.reduce((acc, id, index) => {
                    acc.set(nodes[index], players[id])
                    return acc
                }, new Map())
            })
        })
    }

    static requiredLines(data) {
        return parseInt(data[0])
    }

    static fromGameInspector(gi, players, nodes) {
        const turnCount = gi.turnCount()
        return Array.from(
            {length: turnCount}
        ).map((_, step) => {
            gi.stepHistory(step)
            return new State({
                ownedBy: nodes.reduce((acc, node) => {
                    const ownerId = gi.nodeOwner(node.id)
                    acc.set(node, players[ownerId])
                    return acc
                }, new Map())
            })
        })
    }
}

class Game {
    constructor({
        id,
        name,
        state,
        players,
        nodes,
        rounds,
        states
    }) {
        this.id = id
        this.name = name
        this.state = state
        this.players = players
        this.nodes = nodes
        this.rounds = rounds
        this.states = states
    }


    static parse(data) {
        const [
            id,
            name,
            state,
            _,
            ...gameContent
        ] = data

        const playerLines = Player.requiredLines(gameContent[0])
        const playerData = gameContent.slice(0, playerLines)
        const players = Player.parse(playerData)

        const nodeLines = Node.requiredLines(
            [gameContent[playerLines]]
        )
        const nodeData = gameContent.slice(
            playerLines,
            playerLines + nodeLines
        )
        const nodes = Node.parse(nodeData)

        const actionData = gameContent.slice(playerLines + nodeLines + nodes.length)
        const actionLines = Action.requiredLines(actionData)
        const rounds = Action.parse(actionData, players, nodes)
        
        const stateData = gameContent.slice(
            playerLines + 
            nodeLines + 
            nodes.length +
            actionLines
        )
        const states = State.parse(stateData, players, nodes)

        return new Game({
            id,
            name,
            state,
            players,
            nodes,
            rounds,
            states
        })
    }

    static fromGameInspector(gi) {
        const players = Player.fromGameInspector(gi)
        const nodes = Node.fromGameInspector(gi)
        const rounds = Action.fromGameInspector(gi, players, nodes)
        const states = State.fromGameInspector(gi, players, nodes)
        const name = gi.name()
        const id = gi.id()

        return new Game({
            id,
            name,
            players,
            nodes,
            rounds,
            states,
        })
    }

    static async loadGame(url) {
        const data = await (await fetch(url)).text()
        const game = Game.parse(data.split("\n"))
        console.log(game)
        return game
    }
}
