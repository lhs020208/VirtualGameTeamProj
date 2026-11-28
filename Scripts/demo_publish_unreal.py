"""Reuse the isolated launcher with separate deployment evidence and asset audit."""
import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
source=root/'Scripts/demo_unreal.py';code=source.read_text(encoding='utf-8')
code=code.replace("out=root/'Docs/Demo20261119'","out=root/'Docs/Demo200kSetup'")
code=code.replace("if mode=='level':args+=", "if mode=='audit':args+=['-run=pythonscript','-script='+str(root/'Scripts/demo_asset_audit.py'),'-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities','-nullrhi']\n    elif mode=='level':args+=")
code=code.replace("str(root/'Scripts/demo_level.py')","str(root/'Scripts/demo_publish_level.py')")
exec(compile(code,str(source),'exec'),{'__name__':'__main__','__file__':str(source)})
