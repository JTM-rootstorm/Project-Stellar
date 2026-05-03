PickupGem = {}

function PickupGem.on_object_collider_enter(event)
  stellar.emit_event('gameplay.collect_pickup', {name = event.collider_name})
end
