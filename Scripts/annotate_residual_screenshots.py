from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
import json
O=Path('Docs/ProcessPlant_20261110_LowPoly/Followup_20261110/Screenshots');font='C:/Windows/Fonts/malgun.ttf';f=ImageFont.truetype(font,24);small=ImageFont.truetype(font,20)
info={
'00_overview':('전체 설비 | 최종 잔류 910개만 표시 (혼합기 내부 19,090개는 생략)','B관 1 / 칭량 261 / 수취→혼합 648 | 모든 입자 직경 4mm 유지',[]),
'01_B_context':('B 공급 연결관 | B 1개 | P101B_RigidRoute_ID44','SI 좌표(m): (0.306216, 1.400075, 2.021507) | 원은 가려진 기록 위치',[(800,500,45)]),
'02_B_close':('B 연결관 확대 | B 1개, ID 19261 | 벽에 가려짐','SI 좌표(m): (0.306216, 1.400075, 2.021507) | 08번 배관 숨김 화면과 같은 시점',[(800,500,40)]),
'03_WH_context':('칭량 호퍼 WH101 | 261개 (A 241 / B 20)','SI 범위(m): X -0.128~0.177 / Y -0.040~0.180 / Z 0.786~0.848',[(830,560,80)]),
'04_WH_close':('칭량 호퍼 콘 경사면 확대 | 261개 (A 241 / B 20)','Collision_WH101_Cone_ID700_to_44_final 근처 | 원본 렌더 표면으로 위치 표시',[(1000,410,95),(760,430,100)]),
'05_RH_context':('수취부→혼합기 | 648개 (A 290 / B 358)','밸브 접합부 168개 + 리듀서/혼합기 입구 480개 | Z 1.312~2.139 m',[(800,401,60),(800,628,35)]),
'06_RH_valve':('수취부 BV272 하부 접합부 | 168개 (648개 중)','BV272_111_Ferrule_neck / BV272_110_Split_body_half | Z 2.13466~2.13895 m',[(800,516,175)]),
'07_RH_inlet':('P202 리듀서→MX-301 입구 | 480개 (648개 중)','형상 진단: 연결 내경 44mm → 충돌 입구 약25mm (아래는 렌더 메시; 치수는 별도 충돌메시 검사값)',[(800,477,90)]),
'08_B_exposed':('B 1개 기록 위치 노출 | P101B_* 렌더만 숨김 (물리 시험 아님)','SI 좌표(m): (0.306216, 1.400075, 2.021507) | 02번과 동일 카메라 / 직경4mm',[(800,500,40)])}
for key,(title,detail,circles) in info.items():
 p=O/(key+'_raw.png')
 if not p.exists():continue
 im=Image.open(p).convert('RGB');canvas=Image.new('RGB',(1600,1190),(17,23,32));canvas.paste(im,(0,130));d=ImageDraw.Draw(canvas)
 d.text((25,12),'기록 좌표 재구성, 실시간 실행 화면 아님 | 관측 t = 38.265279033 s',font=f,fill='#ffda71')
 d.text((25,54),title,font=f,fill='white');d.text((25,94),detail,font=small,fill='#cddaea')
 for x,y,r in circles:d.ellipse((x-r,y+130-r,x+r,y+130+r),outline='#ffdf44',width=3)
 d.text((25,1142),'별도 임시 Unreal 캡처 | 기존 전면 절단 표시 / 프레임 숨김 | 빨강 A · 파랑 B | 노란 원은 설명 표식',font=small,fill='white')
 canvas.save(O/(key+'_annotated.png'))
print('annotated',len(list(O.glob('*_annotated.png'))))
