mutable struct SuggestedActions
    index::Int
    actions::Array{Int32,1}
    moving_loc_hs::HexSet
    goal_loc_hs::HexSet
end

const DUMMY_SUGGESTED_ACTIONS::SuggestedActions = SuggestedActions(
    0, UnsafeArray{Int32,1}(pointer(Int32[1]), size(Int32[1])), HexSet(), HexSet()
)

function SuggestedActions(buffer::AbstractVector{Int32}, board::Board)
    return SuggestedActions(0, buffer, HexSet(), HexSet())
end

function add!(sa::SuggestedActions, as_index::Int32)
    if as_index == Int32(-1)
        return nothing
    end
    Intsect.do_for_action(as_index, action -> add!(sa, action, as_index))
    return nothing
end

function add!(sa::SuggestedActions, placement::Placement, as_index::Int32)
    goal_loc = placement.goal_loc
    set!(sa.goal_loc_hs, goal_loc)

    sa.actions[sa.index += 1] = as_index
    if sa.index >= length(sa.actions)
        sa.index -= length(sa.actions)
    end
    return nothing
end

function add!(sa::SuggestedActions, action::Action, as_index::Int32)
    moving_loc = action.moving_loc
    goal_loc = action.goal_loc
    set!(sa.moving_loc_hs, moving_loc)
    set!(sa.goal_loc_hs, goal_loc)

    sa.actions[sa.index += 1] = as_index
    if sa.index >= length(sa.actions)
        sa.index -= length(sa.actions)
    end
    return nothing
end

function add!(sa::SuggestedActions, action::Pass, as_index::Int32)
    # A pass is not a suggested action, it's forces
    return nothing
end

function Base.contains(as_index::Int32, sa::SuggestedActions)
    if as_index == Int32(-1)
        return false
    end
    if as_index <= MAX_PLACEMENT_INDEX
        # Assume that a placement is never a suggested action
        action = ALL_PLACEMENTS[as_index]
        return contains_placement(as_index, action, sa)
    elseif as_index <= MAX_PLACEMENT_INDEX + MAX_MOVEMENT_INDEX
        action = ALL_MOVEMENTS[as_index - MAX_PLACEMENT_INDEX]
        return contains_move(as_index, action, sa)
    elseif as_index <= MAX_PLACEMENT_INDEX + MAX_MOVEMENT_INDEX + MAX_CLIMB_INDEX
        action = ALL_CLIMBS[as_index - (MAX_PLACEMENT_INDEX + MAX_MOVEMENT_INDEX)]
        return contains_move(as_index, action, sa)
    else
        return false
    end
end

function contains_placement(as_index, placement::Placement, sa::SuggestedActions)
    goal_loc = placement.goal_loc
    if sa.goal_loc_hs[goal_loc]
        return as_index in sa.actions
    end
    return false
end

function contains_move(as_index, action, sa::SuggestedActions)
    moving_loc = action.moving_loc
    goal_loc = action.goal_loc
    if sa.moving_loc_hs[moving_loc] && sa.goal_loc_hs[goal_loc]
        return as_index in sa.actions
    end
    return false
end
