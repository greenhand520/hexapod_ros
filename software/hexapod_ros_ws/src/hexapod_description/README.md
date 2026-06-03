## 新建Markdown

```xml
<joint name="coxa_L1" type="revolute">
  <limit lower="-1.047" upper="1.047" />  <!-- ±60° -->
</joint>

<joint name="femur_L1" type="revolute">
  <limit lower="-1.57" upper="0.52" />  <!-- 向下最多90°, 向上30° -->
</joint>

<joint name="tibia_L1" type="revolute">
  <limit lower="-0.52" upper="2.09" />  <!-- 弯曲范围 -->
</joint>

```
