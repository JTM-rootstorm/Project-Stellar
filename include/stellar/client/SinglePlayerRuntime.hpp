#pragma once

#include <cstdint>
#include <optional>

#include "stellar/authority/AuthorityBootstrap.hpp"
#include "stellar/client/ClientRuntime.hpp"
#include "stellar/client/MovementInputMapper.hpp"
#include "stellar/network/Messages.hpp"
#include "stellar/platform/Input.hpp"

namespace stellar::client {

/** @brief Configuration for in-process single-player authoritative simulation. */
struct SinglePlayerRuntimeConfig {
    /** @brief Input mapper configuration used to convert local input to authority commands. */
    MovementInputMapperConfig input_mapper{};

    /** @brief Look mapper configuration used to submit authoritative view angles. */
    LookInputMapperConfig look_mapper{};

    /** @brief Maximum fixed authoritative ticks allowed during one update call. */
    int max_ticks_per_update = 4;

    /** @brief Fixed authoritative timestep used by the in-process single-player driver. */
    float fixed_dt_seconds = 0.0F;
};

/**
 * @brief True single-player runtime that ticks local authority without transport or handshakes.
 *
 * The runtime owns prepared authoritative map/session state and exposes protocol DTO snapshots and
 * gameplay events to presentation. It does not create client/server transports, server runtimes,
 * socket listeners, ClientHello messages, or ServerWelcome/session handshake state.
 */
class SinglePlayerRuntime : public IClientRuntime {
public:
    /** @brief Construct over fully prepared authoritative world/session ownership. */
    explicit SinglePlayerRuntime(stellar::authority::PreparedAuthority authority,
                                 SinglePlayerRuntimeConfig config = {});

    SinglePlayerRuntime(const SinglePlayerRuntime&) = delete;
    SinglePlayerRuntime& operator=(const SinglePlayerRuntime&) = delete;
    /** @brief Move a single-player runtime and its authority ownership. */
    SinglePlayerRuntime(SinglePlayerRuntime&& other) noexcept = default;
    /** @brief Move-assign a single-player runtime and its authority ownership. */
    SinglePlayerRuntime& operator=(SinglePlayerRuntime&& other) noexcept = default;
    ~SinglePlayerRuntime() override = default;

    /** @brief Convert local input, tick authority directly, and return presentation frame data. */
    [[nodiscard]] ClientRuntimeFrame update(const stellar::platform::Input& input,
                                            float delta_seconds) noexcept override;

    /** @brief Return the concrete in-process single-player runtime mode. */
    [[nodiscard]] ClientRuntimeMode mode() const noexcept override;

    /** @brief Return the latest authoritative protocol snapshot, including the initial snapshot. */
    [[nodiscard]] const stellar::network::NetworkWorldSnapshot& latest_snapshot() const noexcept;

    /** @brief Return the local authoritative player id used for presentation lookup. */
    [[nodiscard]] stellar::network::PlayerId local_player_id() const noexcept;

private:
    stellar::authority::PreparedAuthority authority_;
    SinglePlayerRuntimeConfig config_{};
    stellar::network::NetworkWorldSnapshot latest_snapshot_{};
    ClientViewState view_state_{};
    float accumulated_seconds_ = 0.0F;
};

} // namespace stellar::client
