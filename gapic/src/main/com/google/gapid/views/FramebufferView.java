/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.gapid.views;

import static com.google.gapid.image.Images.noAlpha;
import static com.google.gapid.util.Loadable.MessageType.Error;
import static com.google.gapid.util.Loadable.MessageType.Info;
import static com.google.gapid.util.Logging.throttleLogRpcError;
import static com.google.gapid.widgets.Widgets.createArrowButton;
import static com.google.gapid.widgets.Widgets.createComposite;
import static com.google.gapid.widgets.Widgets.createLabel;
import static com.google.gapid.widgets.Widgets.createSeparator;
import static com.google.gapid.widgets.Widgets.createToggleToolItem;
import static com.google.gapid.widgets.Widgets.exclusiveSelection;
import static com.google.gapid.widgets.Widgets.withLayoutData;

import com.google.common.collect.Lists;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.gapid.image.FetchedImage;
import com.google.gapid.image.MultiLayerAndLevelImage;
import com.google.gapid.models.Analytics.View;
import com.google.gapid.models.Capture;
import com.google.gapid.models.CommandStream;
import com.google.gapid.models.CommandStream.CommandIndex;
import com.google.gapid.models.Devices;
import com.google.gapid.models.Follower;
import com.google.gapid.models.Models;
import com.google.gapid.models.Settings;
import com.google.gapid.proto.device.Device;
import com.google.gapid.proto.service.Service;
import com.google.gapid.proto.service.Service.ClientAction;
import com.google.gapid.proto.service.path.Path;
import com.google.gapid.rpc.Rpc;
import com.google.gapid.rpc.RpcException;
import com.google.gapid.rpc.SingleInFlight;
import com.google.gapid.rpc.UiErrorCallback;
import com.google.gapid.server.Client.DataUnavailableException;
import com.google.gapid.util.Loadable;
import com.google.gapid.util.Messages;
import com.google.gapid.util.Paths;
import com.google.gapid.widgets.Balloon;
import com.google.gapid.widgets.ImagePanel;
import com.google.gapid.widgets.LoadableImage;
import com.google.gapid.widgets.LoadingIndicator;
import com.google.gapid.widgets.Theme;
import com.google.gapid.widgets.Widgets;

import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.internal.DPIUtil;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.ToolBar;
import org.eclipse.swt.widgets.Widget;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.function.Supplier;
import java.util.logging.Logger;

/**
 * View that displays the framebuffer at the current selection in an {@link ImagePanel}.
 */
public class FramebufferView extends Composite
    implements Tab, Capture.Listener, Devices.Listener, CommandStream.Listener, Follower.Listener {
  protected static final Logger LOG = Logger.getLogger(FramebufferView.class.getName());
  private static final int MAX_SIZE = 0xffff;
  protected static final int THUMB_SIZE = DPIUtil.autoScaleUp(42);

  private enum RenderSetting {
    RENDER_SHADED(MAX_SIZE, MAX_SIZE, Path.DrawMode.NORMAL),
    RENDER_OVERLAY(MAX_SIZE, MAX_SIZE, Path.DrawMode.WIREFRAME_OVERLAY),
    RENDER_WIREFRAME(MAX_SIZE, MAX_SIZE, Path.DrawMode.WIREFRAME_ALL),
    RENDER_OVERDRAW(MAX_SIZE, MAX_SIZE, Path.DrawMode.OVERDRAW);

    public final int maxWidth;
    public final int maxHeight;
    public final Path.DrawMode drawMode;

    private RenderSetting(int maxWidth, int maxHeight, Path.DrawMode drawMode) {
      this.maxWidth = maxWidth;
      this.maxHeight = maxHeight;
      this.drawMode = drawMode;
    }

    public Path.RenderSettings getRenderSettings(Settings settings) {
      return Paths.renderSettings(maxWidth, maxHeight, drawMode,
        settings.preferences().getDisableReplayOptimization());
    }
  }

  protected final Models models;
  private final SingleInFlight rpcController = new SingleInFlight();
  protected final AttachmentPicker picker;
  protected final ImagePanel imagePanel;
  private RenderSetting renderSettings;

  public FramebufferView(Composite parent, Models models, Widgets widgets) {
    super(parent, SWT.NONE);
    this.models = models;

    setLayout(new GridLayout(2, false));

    ToolBar toolBar = withLayoutData(createToolBar(widgets.theme),
        new GridData(SWT.FILL, SWT.FILL, false, true));
    Composite content = withLayoutData(createComposite(this, new GridLayout(1, false)),
        new GridData(SWT.FILL, SWT.FILL, true, true));

    picker = withLayoutData(new AttachmentPicker(content, widgets, this::updateBuffer),
        new GridData(SWT.FILL, SWT.TOP, true, false));
    imagePanel = withLayoutData(
        new ImagePanel(content, View.Framebuffer, models.analytics, widgets, true),
        new GridData(SWT.FILL, SWT.FILL, true, true));

    imagePanel.createToolbar(toolBar, widgets.theme);
    // Work around for https://bugs.eclipse.org/bugs/show_bug.cgi?id=517480
    Widgets.createSeparator(toolBar);

    renderSettings = RenderSetting.RENDER_SHADED;

    models.capture.addListener(this);
    models.devices.addListener(this);
    models.commands.addListener(this);
    models.follower.addListener(this);
    addListener(SWT.Dispose, e -> {
      models.capture.removeListener(this);
      models.devices.removeListener(this);
      models.commands.removeListener(this);
      models.follower.removeListener(this);
    });
  }

  private ToolBar createToolBar(Theme theme) {
    ToolBar bar = new ToolBar(this, SWT.VERTICAL | SWT.FLAT);
    exclusiveSelection(
        createToggleToolItem(bar, theme.wireframeNone(), e -> {
          models.analytics.postInteraction(View.Framebuffer, ClientAction.Shaded);
          renderSettings = RenderSetting.RENDER_SHADED;
          updateBuffer();
        }, "Render shaded geometry"),
        createToggleToolItem(bar, theme.wireframeOverlay(), e -> {
          models.analytics.postInteraction(View.Framebuffer, ClientAction.OverlayWireframe);
          renderSettings = RenderSetting.RENDER_OVERLAY;
          updateBuffer();
        }, "Render shaded geometry and overlay wireframe of last draw call"),
        createToggleToolItem(bar, theme.wireframeAll(), e -> {
          models.analytics.postInteraction(View.Framebuffer, ClientAction.Wireframe);
          renderSettings = RenderSetting.RENDER_WIREFRAME;
          updateBuffer();
        }, "Render wireframe geometry"),
        createToggleToolItem(bar, theme.overdraw(), e -> {
          models.analytics.postInteraction(View.Framebuffer, ClientAction.Overdraw);
          renderSettings = RenderSetting.RENDER_OVERDRAW;
          updateBuffer();
        }, "Render overdraw"));
    createSeparator(bar);
    return bar;
  }

  @Override
  public Control getControl() {
    return this;
  }

  @Override
  public void reinitialize() {
    if (!models.capture.isLoaded()) {
      onCaptureLoadingStart(false);
    } else {
      loadBuffer();
    }
  }

  @Override
  public void onCaptureLoadingStart(boolean maintainState) {
    imagePanel.setImage(null);
    imagePanel.showMessage(Info, Messages.LOADING_CAPTURE);
    picker.reset();
  }

  @Override
  public void onCaptureLoaded(Loadable.Message error) {
    if (error != null) {
      imagePanel.setImage(null);
      imagePanel.showMessage(Error, Messages.CAPTURE_LOAD_FAILURE);
    }
    picker.reset();
  }

  @Override
  public void onCommandsLoaded() {
    loadBuffer();
  }

  @Override
  public void onCommandsSelected(CommandIndex range) {
    loadBuffer();
  }

  @Override
  public void onReplayDeviceChanged(Device.Instance dev) {
    loadBuffer();
  }

  @Override
  public void onFramebufferAttachmentFollowed(Path.FramebufferAttachment path) {
    picker.selectAttachment(path.getIndex());
  }

  private void loadBuffer() {
    imagePanel.startLoading();

    CommandIndex command = models.commands.getSelectedCommands();
    if (command == null) {
      imagePanel.showMessage(Info, Messages.SELECT_COMMAND);
    } else if (!models.devices.hasReplayDevice()) {
      imagePanel.showMessage(Error, Messages.NO_REPLAY_DEVICE);
    } else if (models.resources.isLoaded()) {
      Rpc.listen(models.resources.loadFramebufferAttachments(),
          new UiErrorCallback<Service.FramebufferAttachments, List<Attachment>, Loadable.Message>(this, LOG) {
        @Override
        protected ResultOrError<List<Attachment>, Loadable.Message> onRpcThread(
            Rpc.Result<Service.FramebufferAttachments> result) {
          try {
            List<Attachment> attachments = Lists.newArrayList();
            for (Service.FramebufferAttachment fba : result.get().getAttachmentsList()) {
              attachments.add(new Attachment(fba.getIndex(), fba.getLabel(), () ->
                  noAlpha(models.images.getThumbnail(command, fba.getIndex(), THUMB_SIZE,
                      i -> { /* noop*/ }))));
            }
            return success(attachments);
          } catch (DataUnavailableException e) {
            return error(Loadable.Message.error(e));
          } catch (RpcException e) {
            models.analytics.reportException(e);
            return error(Loadable.Message.error(e));
          } catch (ExecutionException e) {
            models.analytics.reportException(e);
            throttleLogRpcError(LOG, "Failed to load framebuffer attachments", e);
            return error(Loadable.Message.error(e.getCause().getMessage()));
          }
        }

        @Override
        protected void onUiThreadSuccess(List<Attachment> attachments) {
          picker.setAttachments(attachments);
        }

        @Override
        protected void onUiThreadError(Loadable.Message message) {
          imagePanel.showMessage(message);
        }
      });
    }
  }

  private void updateBuffer() {
    CommandIndex command = models.commands.getSelectedCommands();
    if (command == null) {
      imagePanel.showMessage(Info, Messages.SELECT_COMMAND);
    } else if (!models.devices.hasReplayDevice()) {
      imagePanel.showMessage(Error, Messages.NO_REPLAY_DEVICE);
    } else {
      imagePanel.startLoading();
      rpcController.start().listen(models.images.getFramebuffer(
          command, picker.getSelected(), renderSettings.getRenderSettings(models.settings)),
          new UiErrorCallback<FetchedImage, MultiLayerAndLevelImage, Loadable.Message>(this, LOG) {
        @Override
        protected ResultOrError<MultiLayerAndLevelImage, Loadable.Message> onRpcThread(
            Rpc.Result<FetchedImage> result) throws RpcException, ExecutionException {
          try {
            return success(result.get());
          } catch (DataUnavailableException e) {
            return error(Loadable.Message.info(e));
          } catch (RpcException e) {
            return error(Loadable.Message.error(e));
          }
        }

        @Override
        protected void onUiThreadSuccess(MultiLayerAndLevelImage result) {
          imagePanel.setImage(result);
        }

        @Override
        protected void onUiThreadError(Loadable.Message message) {
          imagePanel.showMessage(message);
        }
      });
    }
  }

  private static class Attachment {
    public final int index;
    public final String label;
    private final Supplier<ListenableFuture<ImageData>> imageSupplier;

    private LoadableImage image;

    public Attachment(
        int index, String label, Supplier<ListenableFuture<ImageData>> imageSupplier) {
      this.index = index;
      this.label = label;
      this.imageSupplier = imageSupplier;
    }

    public Image getImage(
        Widgets widgets, Widget widget, LoadingIndicator.Repaintable repaintable) {
      if (image == null) {
        image = LoadableImage.newBuilder(widgets.loading)
            .small()
            .forImageData(imageSupplier)
            .onErrorShowErrorIcon(widgets.theme)
            .build(widget, repaintable);
      }
      return image.getImage();
    }

    public void dispose() {
      if (image != null) {
        image.dispose();
      }
    }
  }

  private static class AttachmentPicker extends Composite implements LoadingIndicator.Repaintable {
    private final Widgets widgets;
    private final Runnable update;

    private final Label image;
    private final Label label;
    private List<Attachment> attachments = Collections.emptyList();
    private int selected;

    public AttachmentPicker(Composite parent, Widgets widgets, Runnable update) {
      super(parent, SWT.BORDER);
      this.widgets = widgets;
      this.update = update;

      setLayout(new GridLayout(3, false));
      setBackground(getDisplay().getSystemColor(SWT.COLOR_LIST_BACKGROUND));

      image = withLayoutData(createLabel(this, ""),
          new GridData(SWT.LEFT, SWT.CENTER, false, false));
      label = withLayoutData(createLabel(this, ""),
          new GridData(SWT.FILL, SWT.CENTER, true, false));
      withLayoutData(createArrowButton(this, false, e -> showPopup()),
          new GridData(SWT.RIGHT, SWT.CENTER, false, false));

      addListener(SWT.MouseDown, e -> showPopup());
      image.addListener(SWT.MouseDown, e -> showPopup());
      label.addListener(SWT.MouseDown, e -> showPopup());
      addListener(SWT.Dispose, e -> disposeAttachments());
    }

    public int getSelected() {
      return selected;
    }

    private void disposeAttachments() {
      for (Attachment attachment : attachments) {
        attachment.dispose();
      }
    }

    private void showPopup() {
      Rectangle size = getClientArea();
      Balloon[] ballon = new Balloon[1];
      ballon[0] = Balloon.createAndShow(this, shell -> {
        Composite contents = createComposite(shell, new GridLayout(2, false));
        contents.setBackground(getDisplay().getSystemColor(SWT.COLOR_LIST_BACKGROUND));

        Label[] hovered = new Label[2];
        for (Attachment attachment : attachments) {
          Label img = withLayoutData(createLabel(contents, ""),
              new GridData(SWT.LEFT, SWT.CENTER, false, false));
          Label txt = withLayoutData(createLabel(contents, attachment.label),
              new GridData(SWT.FILL, SWT.CENTER, true, false));

          img.setImage(attachment.getImage(widgets, contents, () -> {
            img.setImage(attachment.getImage(widgets, contents, this));
            img.requestLayout();
            shell.setSize(shell.computeSize(size.width, SWT.DEFAULT));
          }));

          Listener listener = e -> {
            if (e.type == SWT.MouseDown) {
              selectAttachment(attachment.index);
              ballon[0].close();
              return;
            }

            if (hovered[0] != txt) {
              if (hovered[0] != null) {
                hovered[0].setForeground(null);
              }
              txt.setForeground(getDisplay().getSystemColor(SWT.COLOR_LIST_SELECTION_TEXT));
              hovered[0] = txt;
              hovered[1] = img;
              contents.redraw();
            }
          };
          img.addListener(SWT.MouseMove, listener);
          img.addListener(SWT.MouseDown, listener);
          txt.addListener(SWT.MouseMove, listener);
          txt.addListener(SWT.MouseDown, listener);

          contents.addListener(SWT.Paint, e -> {
            if (hovered[0] != null && hovered[1] != null) {
              int w = contents.getClientArea().width;
              Rectangle txtBounds = hovered[0].getBounds();
              Rectangle imgBounds = hovered[1].getBounds();
              int y = Math.min(txtBounds.y, imgBounds.y);
              int h = Math.max(txtBounds.y + txtBounds.height, imgBounds.y + imgBounds.height) - y;
              e.gc.setBackground(getDisplay().getSystemColor(SWT.COLOR_LIST_SELECTION));
              e.gc.fillRectangle(0, y, w, h);
            }
          });

        }
      }, new Point(0, size.height), size.width, SWT.DEFAULT);
    }

    public void reset() {
      disposeAttachments();

      attachments = Collections.emptyList();
      selected = 0;
      updateUi();
    }

    public void setAttachments(List<Attachment> attachments) {
      disposeAttachments();

      this.attachments = attachments;
      this.selected = attachments.isEmpty() ? 0 : Math.min(selected, attachments.size() - 1);
      updateUi();
      update.run();
    }

    public void selectAttachment(int index) {
      this.selected = attachments.isEmpty() ? 0 : Math.min(index, attachments.size() - 1);
      updateUi();
      update.run();
    }

    private void updateUi() {
      if (attachments.isEmpty()) {
        image.setImage(null);
        label.setText("");
      } else {
        Attachment attachment = attachments.get(selected);

        image.setImage(attachment.getImage(widgets, this, this));
        label.setText(attachment.label);
        image.requestLayout();
        label.requestLayout();
      }
    }

    @Override
    public void repaint() {
      updateUi();
    }
  }
}
