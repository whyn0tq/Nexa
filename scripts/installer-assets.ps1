$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot "installer/assets"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$logo = [Drawing.Image]::FromFile((Join-Path $projectRoot "assets/nexa-icon.png"))

try {
    foreach ($name in "header", "wizard") {
        $width = if ($name -eq "header") { 128 } else { 440 }
        $height = if ($name -eq "header") { 128 } else { 850 }
        $bitmap = [Drawing.Bitmap]::new($width, $height)
        $canvas = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $canvas.Clear([Drawing.ColorTranslator]::FromHtml('#202020'))
            $canvas.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $canvas.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            if ($name -eq "header") {
                $canvas.DrawImage($logo, [Drawing.Rectangle]::new(8, 8, 112, 112))
            } else {
                $canvas.Clear([Drawing.ColorTranslator]::FromHtml('#111113'))
                $canvas.DrawImage($logo, [Drawing.Rectangle]::new(52, 245, 336, 336))
                $canvas.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
                $title = [Drawing.Font]::new('Segoe UI', 44, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
                $caption = [Drawing.Font]::new('Segoe UI', 21, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
                $text = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#f2f3f5'))
                $accent = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#8b95f7'))
                try {
                    $canvas.DrawString('Nexa', $title, $text, 42, 48)
                    $canvas.DrawString('SFTP / FTP / FTPS', $caption, $accent, 44, 774)
                } finally {
                    $title.Dispose()
                    $caption.Dispose()
                    $text.Dispose()
                    $accent.Dispose()
                }
            }
            $bitmap.Save((Join-Path $outputDir "$name.bmp"), [Drawing.Imaging.ImageFormat]::Bmp)
        } finally {
            $canvas.Dispose()
            $bitmap.Dispose()
        }
    }
} finally {
    $logo.Dispose()
}
