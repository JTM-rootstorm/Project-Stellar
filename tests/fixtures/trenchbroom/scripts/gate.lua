GateTrigger = {}

function GateTrigger.on_trigger_enter(event)
  stellar.emit_event('collision.set_mesh_enabled', {mesh = 'GateDoor', enabled = false})
end
