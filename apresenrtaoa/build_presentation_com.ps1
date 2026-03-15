$ErrorActionPreference = 'Stop'

$base = 'C:\Users\xfalc\Desktop\projeto\apresenrtaoa'
$charts = 'C:\Users\xfalc\Desktop\projeto\charts'
$pptxPath = Join-Path $base 'aco_optimization_10min_final.pptx'
$pdfPath = Join-Path $base 'aco_optimization_10min_final.pdf'

$ppLayoutBlank = 12
$msoTextOrientationHorizontal = 1
$msoShapeRectangle = 1
$msoShapeRoundedRectangle = 5
$msoFalse = 0
$msoTrue = -1

function RGB-Color([int]$r, [int]$g, [int]$b) {
    return ($r + 256 * $g + 65536 * $b)
}

function Add-Panel($slide, [double]$left, [double]$top, [double]$width, [double]$height, [int]$fillRgb, [int]$lineRgb, [int]$shapeType = 5) {
    $shape = $slide.Shapes.AddShape($shapeType, $left, $top, $width, $height)
    $shape.Fill.Visible = $msoTrue
    $shape.Fill.ForeColor.RGB = $fillRgb
    $shape.Line.Visible = $msoTrue
    $shape.Line.ForeColor.RGB = $lineRgb
    return $shape
}

function Add-Textbox($slide, [double]$left, [double]$top, [double]$width, [double]$height, [string]$text, [int]$size, [string]$font, [int]$rgb, [bool]$bold = $false, [int]$align = 1) {
    $shape = $slide.Shapes.AddTextbox($msoTextOrientationHorizontal, $left, $top, $width, $height)
    $shape.TextFrame.WordWrap = $msoTrue
    $shape.TextFrame.MarginLeft = 0
    $shape.TextFrame.MarginRight = 0
    $shape.TextFrame.MarginTop = 0
    $shape.TextFrame.MarginBottom = 0
    $range = $shape.TextFrame.TextRange
    $range.Text = $text
    $range.Font.Name = $font
    $range.Font.Size = $size
    $range.Font.Bold = $(if ($bold) { $msoTrue } else { $msoFalse })
    $range.Font.Color.RGB = $rgb
    $range.ParagraphFormat.Alignment = $align
    return $shape
}

function Add-Title($slide, [string]$section, [string]$title) {
    Add-Textbox $slide 52 24 240 18 $section 10 'Aptos' (RGB-Color 104 116 139) $true 1 | Out-Null
    Add-Textbox $slide 52 42 760 34 $title 23 'Aptos Display' (RGB-Color 20 27 45) $true 1 | Out-Null
    $line = $slide.Shapes.AddShape($msoShapeRectangle, 52, 84, 68, 4)
    $line.Fill.ForeColor.RGB = (RGB-Color 8 145 178)
    $line.Line.Visible = $msoFalse
}

function Add-BulletPanel($slide, [double]$left, [double]$top, [double]$width, [double]$height, [string]$title, [string[]]$bullets) {
    Add-Panel $slide $left $top $width $height (RGB-Color 255 255 255) (RGB-Color 220 226 235) | Out-Null
    Add-Textbox $slide ($left + 14) ($top + 12) ($width - 28) 18 $title 16 'Aptos' (RGB-Color 20 27 45) $true 1 | Out-Null
    $text = ($bullets | ForEach-Object { "- $_" }) -join "`r`n"
    Add-Textbox $slide ($left + 14) ($top + 42) ($width - 28) ($height - 54) $text 15 'Aptos' (RGB-Color 56 66 86) $false 1 | Out-Null
}

function Add-MetricCard($slide, [double]$left, [double]$top, [double]$width, [double]$height, [string]$label, [string]$value, [string]$note, [int]$accentRgb) {
    Add-Panel $slide $left $top $width $height (RGB-Color 255 255 255) (RGB-Color 220 226 235) | Out-Null
    $tag = Add-Panel $slide ($left + 12) ($top + 10) 108 26 $accentRgb $accentRgb
    $tag.TextFrame.TextRange.Text = $label
    $tag.TextFrame.TextRange.Font.Name = 'Aptos'
    $tag.TextFrame.TextRange.Font.Size = 11
    $tag.TextFrame.TextRange.Font.Bold = $msoTrue
    $tag.TextFrame.TextRange.Font.Color.RGB = (RGB-Color 255 255 255)
    $tag.TextFrame.TextRange.ParagraphFormat.Alignment = 2
    Add-Textbox $slide ($left + 14) ($top + 44) ($width - 28) 24 $value 18 'Aptos' (RGB-Color 20 27 45) $true 1 | Out-Null
    Add-Textbox $slide ($left + 14) ($top + 70) ($width - 28) ($height - 74) $note 12 'Aptos' (RGB-Color 104 116 139) $false 1 | Out-Null
}

function Add-CodePanel($slide, [double]$left, [double]$top, [double]$width, [double]$height, [string]$title, [string]$code) {
    Add-Panel $slide $left $top $width $height (RGB-Color 239 244 250) (RGB-Color 220 226 235) | Out-Null
    Add-Textbox $slide ($left + 14) ($top + 12) ($width - 28) 18 $title 13 'Aptos' (RGB-Color 20 27 45) $true 1 | Out-Null
    Add-Textbox $slide ($left + 14) ($top + 36) ($width - 28) ($height - 44) $code 10 'Consolas' (RGB-Color 56 66 86) $false 1 | Out-Null
}

function Add-Band($slide, [double]$left, [double]$top, [double]$width, [double]$height, [string]$text, [int]$fillRgb, [int]$lineRgb) {
    $band = Add-Panel $slide $left $top $width $height $fillRgb $lineRgb
    $band.TextFrame.TextRange.Text = $text
    $band.TextFrame.TextRange.Font.Name = 'Aptos'
    $band.TextFrame.TextRange.Font.Size = 14
    $band.TextFrame.TextRange.Font.Bold = $msoTrue
    $band.TextFrame.TextRange.Font.Color.RGB = (RGB-Color 20 27 45)
    $band.TextFrame.TextRange.ParagraphFormat.Alignment = 2
}

Get-Process POWERPNT -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$ppt = New-Object -ComObject PowerPoint.Application
$ppt.Visible = $msoTrue
$presentation = $ppt.Presentations.Add()
$presentation.PageSetup.SlideWidth = 960
$presentation.PageSetup.SlideHeight = 540

# Slide 1
$slide = $presentation.Slides.Add(1, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid()
$slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Panel $slide 48 64 860 412 (RGB-Color 255 255 255) (RGB-Color 220 226 235) | Out-Null
Add-Textbox $slide 72 90 670 42 'Optimization of an Ant Colony Simulation' 27 'Aptos Display' (RGB-Color 20 27 45) $true 1 | Out-Null
Add-Textbox $slide 72 145 620 28 'Data layout reorganization, OpenMP parallelization, and MPI parallelization' 17 'Aptos' (RGB-Color 56 66 86) $false 1 | Out-Null
Add-Textbox $slide 72 192 680 40 'Objective: reduce total compute time while preserving the simulation model and benchmark fairness.' 16 'Aptos' (RGB-Color 104 116 139) $false 1 | Out-Null
foreach ($chip in @(@{L=72;T='SoA';C=(RGB-Color 8 145 178)}, @{L=178;T='OpenMP';C=(RGB-Color 22 163 74)}, @{L=326;T='MPI';C=(RGB-Color 234 88 12)})) {
    $shape = Add-Panel $slide $chip.L 314 96 44 $chip.C $chip.C
    $shape.TextFrame.TextRange.Text = $chip.T
    $shape.TextFrame.TextRange.Font.Name = 'Aptos'
    $shape.TextFrame.TextRange.Font.Size = 14
    $shape.TextFrame.TextRange.Font.Bold = $msoTrue
    $shape.TextFrame.TextRange.Font.Color.RGB = (RGB-Color 255 255 255)
    $shape.TextFrame.TextRange.ParagraphFormat.Alignment = 2
}
Add-Textbox $slide 650 296 170 68 "AMD Ryzen 5 9600X`r`nWindows, g++ 14.2.0`r`nHeadless benchmark" 14 'Aptos' (RGB-Color 56 66 86) $false 1 | Out-Null

# Slide 2
$slide = $presentation.Slides.Add(2, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Context' 'Problem and Benchmark Setup'
Add-BulletPanel $slide 52 112 456 360 'Benchmark conditions' @(
    'Terrain: 513 x 513 fractal grid, with nest and food source.',
    'Reference workload: 5000 ants, headless execution, identical rules for all methods.',
    'Dominant phase: ant movement, due to random choices and irregular memory access.',
    'Correctness fixes before measurement: seed initialization and pheromone buffer refresh.'
)
Add-MetricCard $slide 524 112 382 112 'Baseline' '0.904 ms/iter' 'Sequential AoS reference' (RGB-Color 8 145 178)
Add-MetricCard $slide 524 246 382 112 'Bottleneck' '0.634 ms/iter' 'Ant movement dominates the runtime' (RGB-Color 22 163 74)
Add-MetricCard $slide 524 380 382 86 'Logic' 'Change -> code -> result' 'Same structure for the three methods' (RGB-Color 234 88 12)

# Slide 3
$slide = $presentation.Slides.Add(3, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Method 1' 'Data Layout Reorganization'
Add-BulletPanel $slide 52 112 312 176 'What changed' @(
    'AoS became SoA: position, state, and seed were separated into arrays.',
    'Movement uses linear indices for terrain and pheromone arrays.',
    'Touched cells are collected first, then updated once in the buffer.'
)
Add-CodePanel $slide 52 306 312 138 'Representative code' "vector<uint32_t> m_land_index, m_pher_index;`r`nvector<uint8_t>  m_loaded;`r`n...`r`nfor (size_t idx : m_touched_indices)`r`n    buffer[idx] = compute_mark(map, idx);"
Add-MetricCard $slide 384 124 152 106 'Total' '0.874 ms/iter' 'vs 0.904 ms/iter' (RGB-Color 8 145 178)
Add-MetricCard $slide 554 124 152 106 'Speedup' '1.03x' 'Small but measurable' (RGB-Color 22 163 74)
Add-MetricCard $slide 724 124 152 106 'Reading' 'Useful' 'But not transformative' (RGB-Color 234 88 12)
Add-Textbox $slide 384 250 492 36 'Interpretation: the layout became cleaner, but the movement kernel remained irregular and only weakly vectorizable.' 15 'Aptos' (RGB-Color 20 27 45) $true 1 | Out-Null
$slide.Shapes.AddPicture((Join-Path $charts 'fig1_fair_comparison.png'), $msoFalse, $msoTrue, 384, 296, 492, 184) | Out-Null

# Slide 4
$slide = $presentation.Slides.Add(4, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Method 2' 'OpenMP Parallelization'
Add-BulletPanel $slide 52 112 292 168 'What changed' @(
    'The final version parallelized ant movement, not only evaporation.',
    'Thread-local mark buffers removed races in the pheromone update.',
    'The merge step remained deterministic.'
)
Add-CodePanel $slide 52 300 292 146 'Representative code' "#pragma omp parallel reduction(+ : food_delta)`r`n{`r`n  ants[i].advance_collect(...);`r`n  local_marks.values[idx] = phen.compute_mark(pos);`r`n}`r`nmerge thread-local marks into the global buffer;"
$slide.Shapes.AddPicture((Join-Path $charts 'fig2_openmp_summary.png'), $msoFalse, $msoTrue, 362, 106, 526, 314) | Out-Null
Add-Band $slide 372 438 514 34 'Best result: 0.412 ms/iter, corresponding to about 2.29x speedup.' (RGB-Color 232 247 236) (RGB-Color 187 247 208)

# Slide 5
$slide = $presentation.Slides.Add(5, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Method 3' 'MPI Parallelization'
Add-BulletPanel $slide 52 112 292 176 'What changed' @(
    'Each process owns only a subset of ants, but keeps the full map.',
    'The pheromone buffer is synchronized with MPI_Allreduce.',
    'Evaporation is distributed by stripes, followed by a second synchronization.'
)
Add-CodePanel $slide 52 306 292 140 'Representative code' "for (...) {`r`n  for (auto& a : ants) a.advance(...);`r`n  MPI_Allreduce(..., MPI_MAX, ...);`r`n  phen.do_evaporation_stripe(...);`r`n  MPI_Allreduce(..., MPI_MIN, ...);`r`n}"
$slide.Shapes.AddPicture((Join-Path $charts 'fig3_mpi_summary.png'), $msoFalse, $msoTrue, 362, 106, 526, 314) | Out-Null
Add-Band $slide 372 438 514 34 'Implemented: MPI approach 1. Approach 2 is discussed as a strategy in the report; the bonus was not pursued.' (RGB-Color 255 244 234) (RGB-Color 254 215 170)

# Slide 6
$slide = $presentation.Slides.Add(6, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Comparison' 'Synthesis in the Common Workload'
$slide.Shapes.AddPicture((Join-Path $charts 'fig1_fair_comparison.png'), $msoFalse, $msoTrue, 58, 112, 640, 320) | Out-Null
Add-MetricCard $slide 740 126 150 104 'SoA' '1.03x' 'Small gain' (RGB-Color 8 145 178)
Add-MetricCard $slide 740 246 150 104 'OpenMP' '2.29x' 'Best practical result' (RGB-Color 22 163 74)
Add-MetricCard $slide 740 366 150 104 'MPI' '< 1x at 5k' 'Needs larger workloads' (RGB-Color 234 88 12)
Add-Textbox $slide 62 448 650 32 'OpenMP is the most effective optimization in the normal regime, while MPI only becomes attractive when the workload is much larger.' 14 'Aptos' (RGB-Color 20 27 45) $true 1 | Out-Null

# Slide 7
$slide = $presentation.Slides.Add(7, $ppLayoutBlank)
$slide.FollowMasterBackground = $msoFalse
$slide.Background.Fill.Solid(); $slide.Background.Fill.ForeColor.RGB = (RGB-Color 245 247 250)
Add-Title $slide 'Takeaways' 'Conclusion'
Add-BulletPanel $slide 66 126 820 304 'Main conclusions' @(
    'The first requirement was correctness: seed initialization and pheromone buffer refresh had to be fixed before any serious comparison.',
    'SoA improved the code organization and produced a small gain, but it did not change the fundamental behavior of the movement kernel.',
    'OpenMP produced the strongest result because it directly targeted the dominant phase of the simulation.',
    'MPI approach 1 was implemented and measured. MPI approach 2 was covered as a proposed strategy, which satisfies the non-bonus requirement of the assignment.'
)
Add-Band $slide 66 448 820 34 'OpenMP gave the best balance between implementation effort and performance gain.' (RGB-Color 255 255 255) (RGB-Color 220 226 235)

$presentation.SaveAs($pptxPath)
$presentation.SaveAs($pdfPath, 32)
$presentation.Close()
$ppt.Quit()

Write-Output "Created $pptxPath"
Write-Output "Created $pdfPath"