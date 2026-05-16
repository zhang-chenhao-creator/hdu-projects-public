"""数据可视化模块"""
import numpy as np
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from sklearn.metrics import ConfusionMatrixDisplay
from .data_loader import CLASS_NAMES_CN


# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'SimSun']
plt.rcParams['axes.unicode_minus'] = False

# 类别颜色
COLORS = ['#FF6B6B', '#4ECDC4', '#45B7D1']
CLASS_LABELS = ['山鸢尾', '变色鸢尾', '弗吉尼亚鸢尾']


def create_scatter_plot(X, y, feature_x=0, feature_y=1, title="鸢尾花数据散点图"):
    """创建散点图

    Args:
        X: 特征矩阵
        y: 标签数组
        feature_x: x轴特征索引
        feature_y: y轴特征索引
        title: 图表标题

    Returns:
        matplotlib Figure
    """
    fig, ax = plt.subplots(figsize=(6, 4.5))

    feature_names = ['花萼长度', '花萼宽度', '花瓣长度', '花瓣宽度']

    for i, (label, color) in enumerate(zip(CLASS_LABELS, COLORS)):
        mask = y == i
        ax.scatter(X[mask, feature_x], X[mask, feature_y],
                   c=color, label=label, alpha=0.7, edgecolors='white', s=50)

    ax.set_xlabel(feature_names[feature_x])
    ax.set_ylabel(feature_names[feature_y])
    ax.set_title(title)
    ax.legend()
    ax.grid(True, alpha=0.3)

    fig.tight_layout()
    return fig


def create_confusion_matrix_plot(y_true, y_pred, title="混淆矩阵"):
    """创建混淆矩阵图

    Args:
        y_true: 真实标签
        y_pred: 预测标签
        title: 图表标题

    Returns:
        matplotlib Figure
    """
    fig, ax = plt.subplots(figsize=(5, 4))

    disp = ConfusionMatrixDisplay.from_predictions(
        y_true, y_pred,
        display_labels=CLASS_LABELS,
        cmap='Blues',
        ax=ax,
        colorbar=False
    )
    ax.set_title(title)

    fig.tight_layout()
    return fig


def create_comparison_bar_chart(results_dict):
    """创建算法准确率对比柱状图

    Args:
        results_dict: {算法名: accuracy}

    Returns:
        matplotlib Figure
    """
    fig, ax = plt.subplots(figsize=(6, 4))

    names = list(results_dict.keys())
    accs = list(results_dict.values())
    bars = ax.bar(names, accs, color=COLORS[:len(names)], alpha=0.8, edgecolor='white')

    # 在柱子上方显示数值
    for bar, acc in zip(bars, accs):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.005,
                f'{acc:.2%}', ha='center', va='bottom', fontsize=10)

    ax.set_ylabel('准确率')
    ax.set_title('算法准确率对比')
    ax.set_ylim(0, 1.1)
    ax.grid(True, axis='y', alpha=0.3)

    fig.tight_layout()
    return fig


def create_feature_distribution_plot(X, y):
    """创建特征分布图（4个子图）

    Args:
        X: 特征矩阵
        y: 标签数组

    Returns:
        matplotlib Figure
    """
    feature_names = ['花萼长度', '花萼宽度', '花瓣长度', '花瓣宽度']

    fig, axes = plt.subplots(2, 2, figsize=(8, 6))

    for idx, (ax, fname) in enumerate(zip(axes.flat, feature_names)):
        for i, (label, color) in enumerate(zip(CLASS_LABELS, COLORS)):
            mask = y == i
            ax.hist(X[mask, idx], bins=15, alpha=0.6, color=color, label=label)
        ax.set_title(fname)
        ax.set_xlabel('值')
        ax.set_ylabel('频数')
        if idx == 0:
            ax.legend(fontsize=8)

    fig.suptitle('各特征分布', fontsize=13)
    fig.tight_layout()
    return fig


def embed_figure_in_tk(canvas_frame, fig):
    """将matplotlib图表嵌入tkinter控件

    Args:
        canvas_frame: tkinter Frame容器
        fig: matplotlib Figure

    Returns:
        FigureCanvasTkAgg
    """
    # 清除旧内容
    for widget in canvas_frame.winfo_children():
        widget.destroy()

    canvas = FigureCanvasTkAgg(fig, master=canvas_frame)
    canvas.draw()
    canvas.get_tk_widget().pack(fill='both', expand=True)
    plt.close(fig)  # 释放内存
    return canvas
