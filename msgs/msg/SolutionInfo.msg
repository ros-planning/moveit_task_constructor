# unique id within task
uint32 id

# associated cost
float32 cost

# associated comment, usually providing failure hint
string comment

# id of stage that created this trajectory
uint32 stage_id

# markers, e.g. providing additional hints or illustrating failure
visualization_msgs/Marker[] markers

# user-defined values that can provide additional information,
# e.g., whether to switch a suction gripper on or off or when to call some other execution interface
Property[] external
