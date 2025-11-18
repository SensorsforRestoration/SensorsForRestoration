"""ESP32 Camera Viewer with Live Preview and Controls

Usage:
  python view_cam.py                           # Live preview window
  python view_cam.py --single                  # Single capture and show
  python view_cam.py --save image.ppm          # Save frame to file
  python view_cam.py --settings brightness=1  # Adjust camera settings

Requirements: requests, pillow, tkinter (built-in)
"""
import argparse
import io
import sys
import time
import threading
import json
from urllib.parse import urlencode

try:
    import requests
    from PIL import Image, ImageTk
    import tkinter as tk
    from tkinter import ttk, messagebox
except Exception as e:
    print("Missing dependencies. Please install from requirements.txt")
    print("Note: tkinter should be built-in with Python")
    raise


class CameraViewer:
    def __init__(self, base_url='http://192.168.4.1'):
        self.base_url = base_url
        self.frame_url = f"{base_url}/frame.ppm"
        self.color_url = f"{base_url}/color.json"
        self.settings_url = f"{base_url}/settings"
        
        self.root = tk.Tk()
        self.root.title("ESP32 Camera Viewer (VGA 640x480)")
        self.root.geometry("800x700")
        
        self.running = False
        self.image_label = None
        self.status_label = None
        
        # Camera settings
        self.brightness = tk.IntVar(value=0)
        self.contrast = tk.IntVar(value=1)
        self.saturation = tk.IntVar(value=0)
        self.sharpness = tk.IntVar(value=1)
        self.pixel_format = tk.IntVar(value=-1)  # -1 = auto-detect
        
        self.setup_ui()
        
    def setup_ui(self):
        # Main frame for image
        img_frame = ttk.Frame(self.root)
        img_frame.pack(padx=10, pady=5, fill='both', expand=True)
        
        self.image_label = ttk.Label(img_frame, text="Click 'Start Live Preview' to begin")
        self.image_label.pack()
        
        # Controls frame
        controls_frame = ttk.LabelFrame(self.root, text="Camera Controls")
        controls_frame.pack(padx=10, pady=5, fill='x')
        
        # Start/Stop buttons
        btn_frame = ttk.Frame(controls_frame)
        btn_frame.pack(pady=5)
        
        self.start_btn = ttk.Button(btn_frame, text="Start Live Preview", command=self.start_preview)
        self.start_btn.pack(side='left', padx=5)
        
        self.stop_btn = ttk.Button(btn_frame, text="Stop Preview", command=self.stop_preview, state='disabled')
        self.stop_btn.pack(side='left', padx=5)
        
        ttk.Button(btn_frame, text="Single Capture", command=self.single_capture).pack(side='left', padx=5)
        ttk.Button(btn_frame, text="Save Image", command=self.save_image).pack(side='left', padx=5)
        
        # Quality settings
        settings_frame = ttk.LabelFrame(controls_frame, text="Image Quality")
        settings_frame.pack(fill='x', padx=5, pady=5)
        
        # Brightness
        ttk.Label(settings_frame, text="Brightness:").grid(row=0, column=0, sticky='w', padx=5)
        brightness_scale = ttk.Scale(settings_frame, from_=-2, to=2, variable=self.brightness, 
                                   command=self.on_brightness_change, orient='horizontal')
        brightness_scale.grid(row=0, column=1, sticky='ew', padx=5)
        self.brightness_label = ttk.Label(settings_frame, text="0")
        self.brightness_label.grid(row=0, column=2, padx=5)
        
        # Contrast
        ttk.Label(settings_frame, text="Contrast:").grid(row=1, column=0, sticky='w', padx=5)
        contrast_scale = ttk.Scale(settings_frame, from_=-2, to=2, variable=self.contrast,
                                 command=self.on_contrast_change, orient='horizontal')
        contrast_scale.grid(row=1, column=1, sticky='ew', padx=5)
        self.contrast_label = ttk.Label(settings_frame, text="1")
        self.contrast_label.grid(row=1, column=2, padx=5)
        
        # Saturation
        ttk.Label(settings_frame, text="Saturation:").grid(row=2, column=0, sticky='w', padx=5)
        saturation_scale = ttk.Scale(settings_frame, from_=-2, to=2, variable=self.saturation,
                                   command=self.on_saturation_change, orient='horizontal')
        saturation_scale.grid(row=2, column=1, sticky='ew', padx=5)
        self.saturation_label = ttk.Label(settings_frame, text="0")
        self.saturation_label.grid(row=2, column=2, padx=5)
        
        # Sharpness
        ttk.Label(settings_frame, text="Sharpness:").grid(row=3, column=0, sticky='w', padx=5)
        sharpness_scale = ttk.Scale(settings_frame, from_=-2, to=2, variable=self.sharpness,
                                  command=self.on_sharpness_change, orient='horizontal')
        sharpness_scale.grid(row=3, column=1, sticky='ew', padx=5)
        self.sharpness_label = ttk.Label(settings_frame, text="1")
        self.sharpness_label.grid(row=3, column=2, padx=5)
        
        # Pixel format
        ttk.Label(settings_frame, text="Pixel Format:").grid(row=4, column=0, sticky='w', padx=5)
        format_combo = ttk.Combobox(settings_frame, textvariable=self.pixel_format, 
                                  values=[-1, 0, 1, 2, 3], state='readonly')
        format_combo.grid(row=4, column=1, sticky='ew', padx=5)
        format_combo.bind('<<ComboboxSelected>>', self.on_format_change)
        ttk.Label(settings_frame, text="Auto/RGB565_LE/BGR565_LE/RGB565_BE/BGR565_BE").grid(row=4, column=2, padx=5)
        
        settings_frame.columnconfigure(1, weight=1)
        
        # Status and color info
        info_frame = ttk.LabelFrame(self.root, text="Camera Info")
        info_frame.pack(padx=10, pady=5, fill='x')
        
        self.status_label = ttk.Label(info_frame, text="Ready")
        self.status_label.pack(pady=5)
        
        self.color_label = ttk.Label(info_frame, text="Center color: Not available")
        self.color_label.pack(pady=2)
        
    def fetch_image(self, params=None):
        url = self.frame_url
        if params:
            url += '?' + urlencode(params)
        else:
            url += f'?ts={int(time.time() * 1000)}'
            
        r = requests.get(url, stream=True, timeout=10)
        r.raise_for_status()
        return Image.open(io.BytesIO(r.content))
        
    def fetch_color_info(self):
        try:
            r = requests.get(self.color_url, timeout=5)
            r.raise_for_status()
            return r.json()
        except:
            return None
            
    def update_setting(self, param, value):
        try:
            params = {param: int(value)}
            r = requests.get(self.settings_url, params=params, timeout=5)
            if r.status_code == 200:
                print(f"Updated {param} = {value}")
        except Exception as e:
            print(f"Failed to update {param}: {e}")
            
    def on_brightness_change(self, value):
        val = int(float(value))
        self.brightness_label.config(text=str(val))
        self.update_setting('brightness', val)
        
    def on_contrast_change(self, value):
        val = int(float(value))
        self.contrast_label.config(text=str(val))
        self.update_setting('contrast', val)
        
    def on_saturation_change(self, value):
        val = int(float(value))
        self.saturation_label.config(text=str(val))
        self.update_setting('saturation', val)
        
    def on_sharpness_change(self, value):
        val = int(float(value))
        self.sharpness_label.config(text=str(val))
        self.update_setting('sharpness', val)
        
    def on_format_change(self, event):
        # Format change will take effect on next image fetch
        pass
        
    def update_image_display(self, pil_image):
        # Resize for display if too large
        display_img = pil_image.copy()
        if display_img.width > 640 or display_img.height > 480:
            display_img.thumbnail((640, 480), Image.Resampling.LANCZOS)
            
        photo = ImageTk.PhotoImage(display_img)
        self.image_label.config(image=photo, text="")
        self.image_label.image = photo  # Keep a reference
        
        # Update status
        self.status_label.config(text=f"Image: {pil_image.width}x{pil_image.height} pixels")
        
    def update_color_info(self):
        info = self.fetch_color_info()
        if info:
            rgb_text = f"RGB=({info['r']},{info['g']},{info['b']}) Color≈{info['name']}"
            self.color_label.config(text=f"Center color: {rgb_text}")
            
    def preview_loop(self):
        while self.running:
            try:
                params = {'ts': int(time.time() * 1000)}
                if self.pixel_format.get() >= 0:
                    params['fmt'] = self.pixel_format.get()
                    
                img = self.fetch_image(params)
                self.root.after(0, self.update_image_display, img)
                self.root.after(0, self.update_color_info)
                
                time.sleep(1)  # 1 FPS for VGA images
                
            except Exception as e:
                self.root.after(0, lambda: self.status_label.config(text=f"Error: {e}"))
                time.sleep(2)
                
    def start_preview(self):
        if not self.running:
            self.running = True
            self.start_btn.config(state='disabled')
            self.stop_btn.config(state='normal')
            self.preview_thread = threading.Thread(target=self.preview_loop, daemon=True)
            self.preview_thread.start()
            
    def stop_preview(self):
        self.running = False
        self.start_btn.config(state='normal')
        self.stop_btn.config(state='disabled')
        
    def single_capture(self):
        try:
            params = {'ts': int(time.time() * 1000)}
            if self.pixel_format.get() >= 0:
                params['fmt'] = self.pixel_format.get()
                
            img = self.fetch_image(params)
            self.update_image_display(img)
            self.update_color_info()
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to capture image: {e}")
            
    def save_image(self):
        try:
            params = {'ts': int(time.time() * 1000)}
            if self.pixel_format.get() >= 0:
                params['fmt'] = self.pixel_format.get()
                
            img = self.fetch_image(params)
            
            from tkinter.filedialog import asksaveasfilename
            filename = asksaveasfilename(
                defaultextension=".png",
                filetypes=[("PNG files", "*.png"), ("PPM files", "*.ppm"), ("All files", "*.*")]
            )
            if filename:
                img.save(filename)
                messagebox.showinfo("Success", f"Image saved to {filename}")
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save image: {e}")
            
    def run(self):
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()
        
    def on_closing(self):
        self.running = False
        self.root.destroy()


def fetch_image_simple(url, timeout=10):
    r = requests.get(url, stream=True, timeout=timeout)
    r.raise_for_status()
    data = r.content
    return Image.open(io.BytesIO(data))


def main():
    p = argparse.ArgumentParser(description='ESP32 Camera Viewer with Live Preview')
    p.add_argument('--url', '-u', default='http://192.168.4.1', help='Base URL (without /frame.ppm)')
    p.add_argument('--single', action='store_true', help='Single capture mode (no GUI)')
    p.add_argument('--save', '-s', default=None, help='Save image to file (single mode)')
    p.add_argument('--format', '-f', type=int, choices=[0,1,2,3], help='Override format')
    p.add_argument('--settings', help='Camera settings as key=value,key=value')
    args = p.parse_args()

    if args.single or args.save:
        # Simple single capture mode
        frame_url = f"{args.url}/frame.ppm"
        params = [f'ts={int(time.time() * 1000)}']
        
        if args.format is not None:
            params.append(f'fmt={args.format}')
            
        url = frame_url + '?' + '&'.join(params)
        
        try:
            img = fetch_image_simple(url)
            print(f'Captured: {img.size[0]}x{img.size[1]} pixels, mode: {img.mode}')
            
            if args.save:
                img.save(args.save)
                print(f'Saved to {args.save}')
            else:
                img.show()
                
        except Exception as e:
            print(f'Error: {e}')
            sys.exit(1)
            
    elif args.settings:
        # Apply settings only
        settings_url = f"{args.url}/settings"
        settings_dict = {}
        for pair in args.settings.split(','):
            key, value = pair.split('=')
            settings_dict[key.strip()] = value.strip()
            
        try:
            r = requests.get(settings_url, params=settings_dict, timeout=10)
            r.raise_for_status()
            print("Settings applied successfully")
        except Exception as e:
            print(f'Error applying settings: {e}')
            sys.exit(1)
    else:
        # GUI mode
        try:
            viewer = CameraViewer(args.url)
            viewer.run()
        except Exception as e:
            print(f"GUI Error: {e}")
            print("Try using --single for command-line mode")
            sys.exit(1)


if __name__ == '__main__':
    main()
