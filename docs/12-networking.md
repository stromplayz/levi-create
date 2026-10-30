# 12 — Networking (custom packets)

Source: https://docs.levimc.org/LeviLamina/api_reference/network/ (canonical: `docs/main/contents/api_reference/network.md`).

Headers: `ll/api/network/packet/Packet.h` (`Packet`, `PacketBase`, `PacketHandlerBase`), `ll/api/network/packet/PacketRegistrar.h` (`PacketRegistrar`, `PacketSide`).

The Network module defines **custom packets between your client mod and server mod**. Before a player joins, LeviLamina checks that both ends handle the packets they need; afterwards each end is kept informed about the other. Protocol: after `PlayerConnectEvent` the server holds back StartGame, exchanges `levilamina:register` packets (fenced by a zero-timestamp `NetworkStackLatencyPacket`); vanilla peers drop the unknown packet harmlessly; required-packet mismatches disconnect with a message listing the missing packets.

## Defining a packet (CRTP — deriving registers it)

```cpp
#include "ll/api/network/packet/Packet.h"

class SetLabelPacket : public ll::network::PacketBase<SetLabelPacket> {
public:
    std::string label;

    void write(BinaryStream& stream) const override { stream.writeString(label, nullptr, nullptr); }

    Bedrock::Result<void> read(ReadOnlyBinaryStream& stream) override {
        auto value = stream.getString(256);
        if (!value) return nonstd::make_unexpected(value.error());
        label = std::move(*value);
        return {};
    }
};
```

## Handling (one instance is created per handler; deriving registers it)

```cpp
class SetLabelHandler : public ll::network::PacketHandlerBase<SetLabelHandler, SetLabelPacket> {
public:
    void handlePacket(
        NetworkIdentifier const& source,
        NetEventCallback&        callback,
        ll::network::PacketSide  side,
        SetLabelPacket const&    packet
    ) const {
        // side == PacketSide::Server means the packet was sent by a client, etc.
    }
};
```

## Sending

```cpp
packet.sendToServer();                        // from a client
packet.sendTo(player);                        // server -> one player (also accepts block pos/dimension/actor)
packet.sendToClient(networkId, subClientId);  // server -> one connection
packet.sendToClients();                       // server -> everyone
```

## Packet identity, version, side, optionality

Default wire id = the C++ **type name** (renaming the class or namespace changes the id!). Pin it explicitly:

```cpp
class SetLabelPacket : public ll::network::PacketBase<SetLabelPacket> {
public:
    static constexpr std::string_view        CustomPacketId{"mymod:set_label"};
    static constexpr uint                    PacketVersion{2};   // bump when wire format changes
    static constexpr ll::network::PacketSide Side{ll::network::PacketSide::Client}; // default: Both
    static constexpr bool                    Optional{true};     // server still works with vanilla clients
    // ...
};
```

- `PacketSide::Server` = sent by clients / handled server-side; `PacketSide::Client` = sent by server; `Both` = default. A client hosting a world is both.
- Packets are **required** unless `Optional{true}`; required packets are login-enforced → safe to send from `PlayerJoinEvent` on (server) or once the world starts loading (client).
- `ll::network::getPacketId<T>` returns the id; `makePacketRuntimeId(id, version)` the on-wire runtime id.

## Peer capability checks (optional packets only)

```cpp
if (packet.canSendTo(player)) packet.sendTo(player);   // server side (also accepts NetworkIdentifier)
if (packet.canSendToServer()) packet.sendToServer();   // client side
// static form (no instance needed): Packet::canSendTo(id, player), Packet::canSendToServer(id)
```

## Runtime registration (script engines)

```cpp
auto& registrar = ll::network::PacketRegistrar::getInstance();
auto  id        = ll::network::makePacketRuntimeId("myscript:ping", 1);
registrar.registerPacket("myscript:ping", id, factory, ll::network::PacketSide::Both, /*optional=*/false, scriptMod);
registrar.registerHandler("myscript:ping", id, handler, ll::network::PacketSide::Both, scriptMod);
// ...
registrar.unregisterHandler(id);
registrar.unregisterPacket(id);
```

Handler must stay alive until unregistered or its mod is unloaded; registrations are dropped when the owning mod unloads.

## Worked example: SimpleTunnel (client↔server message channel)

The full example from the docs combines a string packet with a per-side message queue and `coro::InterruptableSleep`: `SimpleTunnel::of(side)` is a per-side singleton; `send()` wraps the packet; `receive()` is a coroutine that sleeps until `push()` interrupts; the handler pushes into the receiving side's tunnel. Packets are handled on the executor's thread, so **no lock is needed**:

```cpp
// server: echo loop
ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
    auto& tunnel = SimpleTunnel::of(ll::network::PacketSide::Server);
    while (true) {
        auto message = co_await tunnel.receive();
        tunnel.send(message.source, "echo: " + message.data);
    }
}).launch(ll::thread::ServerThreadExecutor::getDefault());

// client: send + await reply
ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
    auto& tunnel = SimpleTunnel::of(ll::network::PacketSide::Client);
    tunnel.send("hello");
    auto reply = co_await tunnel.receive();   // reply.data == "echo: hello"
}).launch(ll::thread::ClientThreadExecutor::getDefault());
```
