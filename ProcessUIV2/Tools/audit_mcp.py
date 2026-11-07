# Execute through Unreal MCP ProgrammaticToolset, not a local Python interpreter.
import json
B="editor_toolset.toolsets.blueprint.BlueprintTools."
U="UMGToolSet.UMGToolSet."
BP={"refPath":"/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2.WBP_ProcessUIV2"}
def call(n,a):
    return execute_tool(n,json.dumps(a,ensure_ascii=False))
def infos(ns):
    return call(B+"get_node_infos",{"nodes":ns})["returnValue"]
def nodes(g):
    return infos(call(B+"find_nodes",{"graph":g,"title":""})["returnValue"])
def link(a,op,b,ip):
    call(B+"connect_pins",{"output_pin":next(p["pin_id"] for p in a["output_pins"] if p["name"]==op),"input_pin":next(p["pin_id"] for p in b["input_pins"] if p["name"]==ip)})
def val(n,k,v):
    call(B+"set_pin_value",{"pin":next(p["pin_id"] for p in n["input_pins"] if p["name"]==k),"value":str(v)})
def create(g,t):
    return infos([call(B+"create_node",{"graph":g,"type_id":t,"pos":{"x":400,"y":0}})["returnValue"]])[0]

def run():
    graphs=call(B+"list_graphs",{"blueprint":BP})["returnValue"]
    exported={g["refPath"].split(":")[-1]:nodes(g) for g in graphs}
    events=exported["EventGraph"]
    bound=[n for n in events if "ComponentBoundEvent" in n["node"]["refPath"]]
    assert len(bound)==31, len(bound)
    for n in bound:
        assert len(next(p for p in n["output_pins"] if p["name"]=="then")["connected_pins"])==1
    for name in ["BeginHover_0","CancelHover","ShowHover","SelectEquipment_0","ClosePanel","Show2D","Show3D","ToggleDetails","ApplyEquipmentSnapshot","ConfigureCommands","OnPreviewKeyDown"]:
        e=next(n for n in exported[name] if "FunctionEntry" in n["node"]["refPath"])
        assert next(p for p in e["output_pins"] if p["name"]=="then")["connected_pins"],name
    timers=[n for n in exported["BeginHover_0"] if "SetTimerbyFunctionName" in n["type_id"]]
    assert len(timers)==1
    timer=timers[0]
    assert float(next(p["value"] for p in timer["input_pins"] if p["name"]=="Time"))==2.5
    ws=call(U+"GetWidgets",{"widgetBlueprint":BP})["returnValue"]
    equipment=[w for w in ws["widgets"] if w["widgetName"] in ["Equipment"+str(i) for i in range(9)]]
    assert len(equipment)==9
    return {"summary":{"equipment":len(equipment),"wired_button_events":len(bound),"hover_seconds":2.5,"widget_count":len(ws["widgets"]),"runtime_input_tested":False},"graphs":exported,"widgets":ws}

