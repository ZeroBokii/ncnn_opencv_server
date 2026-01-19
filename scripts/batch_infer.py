#!/usr/bin/env python3
"""
批量推理脚本 - 使用NCNN执行YOLO11推理
支持模块：
  - annotate: 生成YOLO格式标注文件
  - visualize: 根据推理结果画图保存
"""

import os
import argparse
import numpy as np
import cv2
import ncnn
from pathlib import Path


class Yolo11Detector:
    """YOLO11 NCNN推理器"""
    
    def __init__(self, param_path: str, bin_path: str, 
                 conf_threshold: float = 0.5, nms_threshold: float = 0.45,
                 target_size: int = 640):
        self.conf_threshold = conf_threshold
        self.nms_threshold = nms_threshold
        self.target_size = target_size
        self.reg_max = 16
        self.strides = [8, 16, 32]
        
        self.net = ncnn.Net()
        self.net.opt.use_vulkan_compute = False
        self.net.load_param(param_path)
        self.net.load_model(bin_path)
        
        print(f"✓ 模型加载成功: {param_path}")
    
    def preprocess(self, image: np.ndarray):
        """Letterbox预处理"""
        orig_h, orig_w = image.shape[:2]
        scale = min(self.target_size / orig_w, self.target_size / orig_h)
        new_w = int(orig_w * scale)
        new_h = int(orig_h * scale)
        wpad = self.target_size - new_w
        hpad = self.target_size - new_h
        
        resized = cv2.resize(image, (new_w, new_h))
        top = hpad // 2
        bottom = hpad - top
        left = wpad // 2
        right = wpad - left
        padded = cv2.copyMakeBorder(resized, top, bottom, left, right, 
                                     cv2.BORDER_CONSTANT, value=(114, 114, 114))
        
        rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)
        mat_in = ncnn.Mat.from_pixels(rgb, ncnn.Mat.PixelType.PIXEL_RGB, 
                                       self.target_size, self.target_size)
        
        mean_vals = [0.0, 0.0, 0.0]
        norm_vals = [1.0/255.0, 1.0/255.0, 1.0/255.0]
        mat_in.substract_mean_normalize(mean_vals, norm_vals)
        
        return mat_in, scale, wpad, hpad, orig_w, orig_h
    
    def softmax(self, x):
        e_x = np.exp(x - np.max(x))
        return e_x / e_x.sum()
    
    def decode_dfl(self, dfl_data):
        prob = self.softmax(dfl_data)
        return np.sum(np.arange(self.reg_max) * prob)
    
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def postprocess(self, output, scale, wpad, hpad, orig_w, orig_h):
        num_proposals = output.shape[0]
        feat_dim = output.shape[1]
        num_classes = feat_dim - self.reg_max * 4
        
        detections = []
        proposal_offset = 0
        
        for stride in self.strides:
            num_grid_x = self.target_size // stride
            num_grid_y = self.target_size // stride
            num_grid = num_grid_x * num_grid_y
            
            for idx in range(num_grid):
                anchor_idx = proposal_offset + idx
                if anchor_idx >= num_proposals:
                    break
                
                pred_data = output[anchor_idx]
                class_scores = pred_data[self.reg_max * 4:]
                max_class_id = np.argmax(class_scores)
                max_score = class_scores[max_class_id]
                confidence = self.sigmoid(max_score)
                
                if confidence < self.conf_threshold:
                    continue
                
                pred_ltrb = []
                for k in range(4):
                    dfl_data = pred_data[k * self.reg_max:(k + 1) * self.reg_max]
                    dist = self.decode_dfl(dfl_data) * stride
                    pred_ltrb.append(dist)
                
                y = idx // num_grid_x
                x = idx % num_grid_x
                center_x = (x + 0.5) * stride
                center_y = (y + 0.5) * stride
                
                x0 = center_x - pred_ltrb[0]
                y0 = center_y - pred_ltrb[1]
                x1 = center_x + pred_ltrb[2]
                y1 = center_y + pred_ltrb[3]
                
                detections.append({
                    'x': x0, 'y': y0,
                    'width': x1 - x0, 'height': y1 - y0,
                    'confidence': confidence,
                    'class_id': int(max_class_id)
                })
            
            proposal_offset += num_grid
        
        final_detections = self.apply_nms(detections)
        
        boxes = []
        for det in final_detections:
            x_orig = (det['x'] - wpad / 2.0) / scale
            y_orig = (det['y'] - hpad / 2.0) / scale
            w_orig = det['width'] / scale
            h_orig = det['height'] / scale
            
            x_orig = max(0, min(x_orig, orig_w - 1))
            y_orig = max(0, min(y_orig, orig_h - 1))
            x1 = x_orig
            y1 = y_orig
            x2 = min(x_orig + w_orig, orig_w - 1)
            y2 = min(y_orig + h_orig, orig_h - 1)
            
            boxes.append({
                'left': x1, 'top': y1, 'right': x2, 'bottom': y2,
                'class_id': det['class_id'], 'score': det['confidence']
            })
        
        return boxes
    
    def apply_nms(self, detections):
        if not detections:
            return []
        
        detections = sorted(detections, key=lambda x: x['confidence'], reverse=True)
        suppressed = [False] * len(detections)
        result = []
        
        for i, det_i in enumerate(detections):
            if suppressed[i]:
                continue
            result.append(det_i)
            for j in range(i + 1, len(detections)):
                if suppressed[j]:
                    continue
                det_j = detections[j]
                if det_i['class_id'] == det_j['class_id']:
                    iou = self.calculate_iou(det_i, det_j)
                    if iou > self.nms_threshold:
                        suppressed[j] = True
        return result
    
    def calculate_iou(self, a, b):
        x1 = max(a['x'], b['x'])
        y1 = max(a['y'], b['y'])
        x2 = min(a['x'] + a['width'], b['x'] + b['width'])
        y2 = min(a['y'] + a['height'], b['y'] + b['height'])
        
        inter_w = max(0, x2 - x1)
        inter_h = max(0, y2 - y1)
        inter_area = inter_w * inter_h
        
        area_a = a['width'] * a['height']
        area_b = b['width'] * b['height']
        union_area = area_a + area_b - inter_area
        
        return inter_area / union_area if union_area > 0 else 0
    
    def infer(self, image: np.ndarray):
        mat_in, scale, wpad, hpad, orig_w, orig_h = self.preprocess(image)
        ex = self.net.create_extractor()
        ex.input("in0", mat_in)
        _, mat_out = ex.extract("out0")
        output = np.array(mat_out)
        boxes = self.postprocess(output, scale, wpad, hpad, orig_w, orig_h)
        return boxes


# ==================== 模块：标注 ====================
def module_annotate(image_files, detector, output_dir, class_id, labels):
    """生成YOLO格式标注文件"""
    os.makedirs(output_dir, exist_ok=True)
    
    success_count = 0
    detection_count = 0
    
    for img_path in image_files:
        try:
            image = cv2.imread(str(img_path))
            if image is None:
                print(f"无法读取: {img_path.name}")
                continue
            
            img_h, img_w = image.shape[:2]
            boxes = detector.infer(image)
            
            annotations = [to_yolo_format(box, img_w, img_h, class_id) for box in boxes]
            
            txt_path = Path(output_dir) / f"{img_path.stem}.txt"
            with open(txt_path, 'w') as f:
                for line in annotations:
                    f.write(line + '\n')
            
            success_count += 1
            detection_count += len(boxes)
                
        except Exception as e:
            print(f"✗ {img_path.name}: {e}")
    
    print(f"[标注模块] 处理: {len(image_files)}, 成功: {success_count}, 检测: {detection_count}")


def to_yolo_format(box, img_width, img_height, default_class_id=0):
    """转换为YOLO归一化坐标格式"""
    x_center = (box['left'] + box['right']) / 2.0 / img_width
    y_center = (box['top'] + box['bottom']) / 2.0 / img_height
    width = (box['right'] - box['left']) / img_width
    height = (box['bottom'] - box['top']) / img_height
    
    x_center = max(0, min(1, x_center))
    y_center = max(0, min(1, y_center))
    width = max(0, min(1, width))
    height = max(0, min(1, height))
    
    return f"{default_class_id} {x_center:.6f} {y_center:.6f} {width:.6f} {height:.6f}"


# ==================== 模块：可视化 ====================
def module_visualize(image_files, detector, input_dir, labels):
    """根据推理结果画图并保存到 out 文件夹"""
    output_dir = Path(input_dir) / "out"
    os.makedirs(output_dir, exist_ok=True)
    
    success_count = 0
    skip_count = 0
    
    for img_path in image_files:
        try:
            image = cv2.imread(str(img_path))
            if image is None:
                print(f"无法读取: {img_path.name}")
                continue
            
            boxes = detector.infer(image)
            
            # 没有检测结果则跳过
            if not boxes:
                skip_count += 1
                continue
            
            # 画图
            vis_image = draw_detections(image, boxes, labels)
            
            # 保存到 out 文件夹
            out_path = output_dir / img_path.name
            cv2.imwrite(str(out_path), vis_image)
            success_count += 1
                
        except Exception as e:
            print(f"✗ {img_path.name}: {e}")
    
    print(f"[可视化模块] 处理: {len(image_files)}, 保存: {success_count}, 跳过(无检测): {skip_count}")


def draw_detections(image, boxes, labels=None):
    """在图像上绘制检测框"""
    vis_image = image.copy()
    
    for box in boxes:
        cls_id = box['class_id']
        score = box['score']
        left = int(box['left'])
        top = int(box['top'])
        right = int(box['right'])
        bottom = int(box['bottom'])
        
        # 根据类别ID生成颜色
        hue = (cls_id * 137) % 180
        color = cv2.cvtColor(np.uint8([[[hue, 255, 255]]]), cv2.COLOR_HSV2BGR)[0][0]
        color = (int(color[0]), int(color[1]), int(color[2]))
        
        # 获取标签名
        if labels and 0 <= cls_id < len(labels):
            label_text = labels[cls_id]
        else:
            label_text = f"class_{cls_id}"
        
        display_text = f"{label_text} {score:.2f}"
        
        # 画框
        cv2.rectangle(vis_image, (left, top), (right, bottom), color, 2)
        
        # 画标签背景
        (text_w, text_h), baseline = cv2.getTextSize(display_text, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 1)
        label_top = max(top, text_h)
        cv2.rectangle(vis_image, (left, label_top - text_h), (left + text_w, label_top + baseline), color, -1)
        
        # 画标签文字
        cv2.putText(vis_image, display_text, (left, label_top), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
    
    return vis_image


def load_labels(label_path):
    """加载标签文件"""
    if not label_path or not os.path.exists(label_path):
        return None
    
    labels = []
    with open(label_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                labels.append(line)
    return labels if labels else None


def get_supported_extensions():
    return {'.jpg', '.jpeg', '.png', '.bmp', '.webp', '.tiff', '.tif'}


def main():
    parser = argparse.ArgumentParser(description='YOLO11 NCNN批量推理工具')
    parser.add_argument('--input', '-i', default='/home/torch/images',
                        help='输入图片目录 (默认: /home/torch/images)')
    parser.add_argument('--output', '-o', default='/home/torch/labels',
                        help='标注输出目录 (默认: /home/torch/labels)')
    parser.add_argument('--param', default='/home/torch/development/ncnn_opencv_server/workspace/models/3dv11n-cfg-v3/best.ncnn.param',
                        help='NCNN param文件路径')
    parser.add_argument('--bin', default='/home/torch/development/ncnn_opencv_server/workspace/models/3dv11n-cfg-v3/best.ncnn.bin',
                        help='NCNN bin文件路径')
    parser.add_argument('--labels', default=None,
                        help='标签文件路径 (用于可视化显示类别名)')
    parser.add_argument('--conf', type=float, default=0.5,
                        help='置信度阈值 (默认: 0.5)')
    parser.add_argument('--nms', type=float, default=0.45,
                        help='NMS阈值 (默认: 0.45)')
    parser.add_argument('--class-id', type=int, default=0,
                        help='默认类别ID (默认: 0)')
    parser.add_argument('--size', type=int, default=640,
                        help='输入尺寸 (默认: 640)')
    
    # 模块开关
    parser.add_argument('--annotate', action='store_true',
                        help='启用标注模块 (生成YOLO格式txt)')
    parser.add_argument('--visualize', action='store_true',
                        help='启用可视化模块 (画图保存到out文件夹)')
    
    args = parser.parse_args()
    
    # 如果没有指定任何模块，默认启用标注模块
    if not args.annotate and not args.visualize:
        args.annotate = True
    
    if not os.path.exists(args.input):
        print(f"错误: 输入目录不存在: {args.input}")
        return
    
    # 初始化检测器
    detector = Yolo11Detector(
        param_path=args.param,
        bin_path=args.bin,
        conf_threshold=args.conf,
        nms_threshold=args.nms,
        target_size=args.size
    )
    
    # 加载标签
    labels = load_labels(args.labels)
    
    print(f"输入目录: {args.input}")
    print(f"置信度阈值: {args.conf}")
    print(f"启用模块: {'标注 ' if args.annotate else ''}{'可视化' if args.visualize else ''}")
    print("-" * 50)
    
    # 获取所有图片
    supported_ext = get_supported_extensions()
    image_files = [f for f in Path(args.input).iterdir() 
                   if f.is_file() and f.suffix.lower() in supported_ext]
    
    if not image_files:
        print("未找到图片文件")
        return
    
    print(f"找到 {len(image_files)} 张图片")
    
    # 执行模块
    if args.annotate:
        module_annotate(image_files, detector, args.output, args.class_id, labels)
    
    if args.visualize:
        module_visualize(image_files, detector, args.input, labels)
    
    print("-" * 50)
    print("批量推理完成")


if __name__ == '__main__':
    main()